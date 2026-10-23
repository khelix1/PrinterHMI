#!/usr/bin/python3
"""Offline Linux desktop shell for the bundled PrinterHMI Theme Maker editor."""
import json
from pathlib import Path
import sys
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor
from threading import current_thread, main_thread
from native_preview import NativePreview

from desktop_io import export_request, atomic_save, atomic_save_bytes, MAX_BYTES

try:
    from artwork import store_source, preview, bundle, import_bundle, MAX_INPUT
    from projects import save_request, pack, unpack, MAX_TEXT, MAX_ARCHIVE
    import gi
    gi.require_version('Gtk', '3.0')
    gi.require_version('WebKit2', '4.1')
    from gi.repository import Gio, GLib, Gtk, WebKit2
except (ImportError, ValueError) as error:
    sys.exit('Theme Maker needs the Linux Mint desktop dependencies.\n'
             'Install the .deb using apt, or run:\n'
             'sudo apt install python3-gi gir1.2-gtk-3.0 gir1.2-webkit2-4.1 python3-pil\n'
             + str(error))

BASE = Path(__file__).resolve().parent.parent
ENTRY = (BASE / 'index.html').as_uri()
APP_ID = 'io.github.khelix1.PrinterHMIThemeMaker'


class ThemeMaker(Gtk.Application):
    def __init__(self):
        super().__init__(application_id=APP_ID, flags=Gio.ApplicationFlags.FLAGS_NONE)
        self.window = None
        self.export_busy = False
        self.native = None
        self.native_path = None
        self.native_probed = False
        self.native_worker = ThreadPoolExecutor(max_workers=1, thread_name_prefix="tablet-preview")

    def native_executable(self):
        if self.native_probed: return self.native_path
        self.native_probed = True
        candidates = [BASE.parents[1] / "build-theme-maker-native/printerhmi-ui-host", Path("/usr/lib/printerhmi-theme-maker/printerhmi-ui-host")]
        for path in candidates:
            if path.is_file():
                try:
                    version=subprocess.check_output([str(path),'--version'],timeout=2,text=True,stdin=subprocess.DEVNULL,stderr=subprocess.DEVNULL).strip()
                    if version=='printerhmi-ui-host 16':
                        self.native_path=path
                        return path
                except (OSError,subprocess.SubprocessError): pass
        return None

    def native_status(self, view, event):
        if event == WebKit2.LoadEvent.FINISHED:
            self.callback("ThemeMakerNativeStatus", {"available": bool(self.native_executable())})

    def do_shutdown(self):
        self.native_worker.shutdown(wait=True, cancel_futures=True)
        if self.native: self.native.close(); self.native = None
        Gtk.Application.do_shutdown(self)

    def do_activate(self):
        if self.window:
            self.window.present()
            return
        self.window = Gtk.ApplicationWindow(application=self)
        self.window.set_title('PrinterHMI Theme Maker')
        self.window.set_icon_name('printerhmi-theme-maker')
        self.window.set_default_size(1440, 900)
        self.window.set_size_request(900, 640)
        data = Path(GLib.get_user_data_dir()) / 'printerhmi-theme-maker'
        cache = Path(GLib.get_user_cache_dir()) / 'printerhmi-theme-maker'
        data.mkdir(parents=True, exist_ok=True)
        cache.mkdir(parents=True, exist_ok=True)
        self.artwork_dir = data / 'artwork'
        manager = WebKit2.WebsiteDataManager(base_data_directory=str(data), base_cache_directory=str(cache))
        context = WebKit2.WebContext.new_with_website_data_manager(manager)
        self.view = WebKit2.WebView.new_with_context(context)
        settings = self.view.get_settings()
        settings.set_enable_html5_local_storage(True)
        settings.set_enable_developer_extras(False)
        settings.set_javascript_can_open_windows_automatically(False)
        self.view.connect('decide-policy', self.policy)
        self.view.connect('permission-request', lambda view, request: (request.deny(), True)[1])
        self.view.connect('create', lambda *args: None)
        self.view.connect('load-changed', self.native_status)
        self.view.connect('load-failed', self.load_failed)
        self.view.connect('web-process-terminated', self.process_terminated)
        content = self.view.get_user_content_manager()
        content.connect('script-message-received::themeMaker', self.message)
        content.register_script_message_handler('themeMaker')
        self.window.add(self.view)
        self.window.show_all()
        self.view.load_uri(ENTRY)

    def policy(self, view, decision, kind):
        if kind == WebKit2.PolicyDecisionType.NAVIGATION_ACTION:
            uri = decision.get_navigation_action().get_request().get_uri()
            if uri == ENTRY or uri.startswith(ENTRY + '#'):
                decision.use()
            else:
                decision.ignore()
            return True
        if kind == WebKit2.PolicyDecisionType.NEW_WINDOW_ACTION:
            decision.ignore()
            return True
        return False

    def notify(self, text, error=False):
        # Only a constant callback and JSON-escaped text cross back into the editor.
        script = 'window.ThemeMakerDesktopResult(' + json.dumps(text) + ',' + json.dumps(error) + ');'
        self.view.evaluate_javascript(script, -1, None, None, None, None, None)

    def callback(self, name, data):
        if current_thread() is not main_thread():
            GLib.idle_add(lambda: (self.callback(name, data), False)[1])
            return
        script = 'window.' + name + '(' + json.dumps(data) + ');'
        self.view.evaluate_javascript(script, -1, None, None, None, None, None)

    def choose_file(self, title, action, patterns, name=None):
        dialog = Gtk.FileChooserDialog(title=title, parent=self.window, action=action)
        dialog.add_buttons('_Cancel', Gtk.ResponseType.CANCEL,
                           '_Save' if action == Gtk.FileChooserAction.SAVE else '_Open', Gtk.ResponseType.ACCEPT)
        if name:
            dialog.set_current_name(name)
            dialog.set_do_overwrite_confirmation(True)
        file_filter = Gtk.FileFilter(); file_filter.set_name(title)
        for pattern in patterns: file_filter.add_pattern(pattern)
        dialog.add_filter(file_filter)
        try:
            response = dialog.run()
            return dialog.get_filename() if response == Gtk.ResponseType.ACCEPT else None
        finally:
            dialog.destroy()

    def native_message(self, request):
        try:
            executable = self.native_executable()
            if not executable: raise ValueError('Native UI helper is not installed.')
            operation = request.get('operation'); setup = []
            if operation == 'reset':
                if self.native: self.native.close(); self.native = None
                return
            if self.native is None: self.native = NativePreview(executable)
            if operation == 'open':
                kind = request.get('kind')
                if kind not in ('nozzle', 'bed', 'fan', 'stop', 'toolhead', 'tune', 'tools-page', 'console-page', 'printer-page', 'dashboard-page', 'files-page', 'camera-page', 'drybox-page', 'settings-page', 'network-page', 'chooser-page', 'telemetry-page', 'devices-page', 'macros-page', 'mesh-page', 'calibration-page'): raise ValueError('Unsupported native popup.')
                palette = request.get('palette')
                if not isinstance(palette, dict) or len(palette) > 64: raise ValueError('Invalid native palette.')
                for name, color in palette.items():
                    if not re.fullmatch(r'[a-z_0-9]{1,48}', name) or not isinstance(color, str) or not re.fullmatch(r'#[0-9a-fA-F]{6}', color): raise ValueError('Invalid native color.')
                    setup.append('color ' + name + ' ' + color[1:])
                metrics = request.get('metrics', {})
                for name in ('radius_card', 'radius_button', 'radius_panel'):
                    value = metrics.get(name)
                    if type(value) is not int or not 0 <= value <= 64: raise ValueError('Invalid native metric.')
                    setup.append('metric ' + name + ' ' + str(value))
                targets = request.get('targets', [210, 60, 65])
                if not isinstance(targets, list) or len(targets) != 3 or any(type(v) not in (int, float) or not 0 <= v <= limit for v, limit in zip(targets, (300, 120, 100))): raise ValueError('Invalid native targets.')
                setup.append('targets ' + ' '.join(str(v) for v in targets))
                position=request.get('position',[110,110,12.8,0])
                if not isinstance(position,list) or len(position)!=4 or any(type(v) not in (int,float) or not -1000<=v<=1000 for v in position):raise ValueError('Invalid native position.')
                setup.append('position '+' '.join(str(v) for v in position))
                factors=request.get('factors',[100,100])
                if not isinstance(factors,list) or len(factors)!=2 or any(type(v) not in (int,float) or not 10<=v<=300 for v in factors):raise ValueError('Invalid native tuning factors.')
                setup.append('factors '+' '.join(str(v) for v in factors))
                state=request.get('printState','printing')
                if state=='stopped':state='shutdown'
                if state not in ('printing','paused','ready','standby','cancelled','complete','shutdown','error','offline'):raise ValueError('Invalid simulated print state.')
                job=request.get('job','Spool holder.gcode')
                if not isinstance(job,str) or len(job.encode('utf-8'))>255 or any(ord(c)<32 for c in job):raise ValueError('Invalid simulated job.')
                progress=request.get('progress',64)
                if type(progress) not in (int,float) or not 0<=progress<=100:raise ValueError('Invalid simulated progress.')
                setup.extend(['state '+state,'job '+job,'progress '+str(progress)])
                line = 'open ' + kind + ' 1'
            elif operation == 'wheel':
                x,y,delta=request.get('x'),request.get('y'),request.get('delta')
                if any(type(v) is not int for v in (x,y,delta)):raise ValueError('Invalid native wheel.')
                line='wheel %d %d %d' % (max(0,min(1023,x)),max(0,min(599,y)),max(-600,min(600,delta)))
            elif operation == 'hover':
                x,y=request.get('x'),request.get('y')
                if type(x) is not int or type(y) is not int: raise ValueError('Invalid native hover.')
                cursor=self.native.hover(max(0,min(1023,x)),max(0,min(599,y)))
                self.callback('ThemeMakerNativeHover', {'token':request.get('token'),'sequence':request.get('sequence'),'cursor':cursor})
                return
            elif operation == 'pointer':
                x, y, down = request.get('x'), request.get('y'), request.get('down')
                if type(x) is not int or type(y) is not int or down not in (0, 1): raise ValueError('Invalid native pointer.')
                line = 'pointer %d %d %d' % (max(0,min(1023,x)), max(0,min(599,y)),down)
            elif operation == 'key':
                key = request.get('key')
                if type(key) is not int or not 0 <= key <= 0x10ffff: raise ValueError('Invalid native key.')
                line = 'key ' + str(key)
            elif operation == 'close': line = 'close'
            elif operation == 'frame': line = 'frame'
            else: raise ValueError('Invalid native UI operation.')
            response = self.native.request(line, setup)
            response['token'] = request.get('token')
            self.callback('ThemeMakerNativeFrame', response)
            return
        except Exception as error:
            if self.native: self.native.close(); self.native = None
            self.callback("ThemeMakerNativeError", {"token": request.get("token"), "message": str(error)})

    def message(self, manager, result):
        if self.view.get_uri() not in (ENTRY, ENTRY + '#'):
            return
        try:
            raw = result.get_js_value().to_string()
            if len(raw.encode('utf-8')) > 2 * MAX_TEXT:
                raise ValueError('Desktop request is too large.')
            request = json.loads(raw)
            if not isinstance(request, dict): raise ValueError('Invalid desktop request.')
        except Exception as error:
            self.notify('Could not complete the action: ' + str(error), True)
            return
        action = request.get('action')
        if action == 'native-ui':
            self.native_worker.submit(self.native_message, request)
            return
        if self.export_busy:
            return
        self.export_busy = True
        try:
            if action == 'save-project':
                name, text = save_request(raw)
                data = pack(text, self.artwork_dir)
                destination = self.choose_file('Save editable Theme Maker project', Gtk.FileChooserAction.SAVE,
                                               ['*.phmiproject'], name)
                if destination:
                    atomic_save_bytes(destination, data)
                    self.notify('Project saved: ' + Path(destination).name + '. Reopen it with Open project.')
                else:
                    self.notify('Project save cancelled. Your draft is unchanged.')
                return
            if action == 'open-project':
                filename = self.choose_file('Open editable Theme Maker project', Gtk.FileChooserAction.OPEN,
                                            ['*.phmiproject'])
                if not filename:
                    self.notify('Open project cancelled. Your draft is unchanged.')
                    return
                path = Path(filename)
                if path.stat().st_size > MAX_ARCHIVE:
                    raise ValueError('Project exceeds 6 MiB.')
                self.callback('ThemeMakerWorkspaceLoaded', {'document': unpack(path.read_bytes(), self.artwork_dir), 'filename': path.name})
                return
            if action == 'choose-artwork':
                filename = self.choose_file('Choose background image', Gtk.FileChooserAction.OPEN,
                                            ['*.png', '*.jpg', '*.jpeg', '*.webp', '*.PNG', '*.JPG', '*.JPEG', '*.WEBP'])
                if filename:
                    path = Path(filename)
                    if path.stat().st_size > MAX_INPUT: raise ValueError('Choose an image no larger than 12 MiB.')
                    asset, size = store_source(path.read_bytes(), self.artwork_dir)
                    self.callback('ThemeMakerArtworkSelected', {'asset': asset, 'width': size[0], 'height': size[1]})
                return
            if action == 'preview-artwork':
                project = {'background': request.get('background'), 'palette': {'background': request.get('color')}}
                self.callback('ThemeMakerArtworkPreview', {'key': request.get('key'), 'data_url': preview(project, self.artwork_dir)})
                return
            if action == 'import-theme':
                filename = self.choose_file('Import theme or artwork bundle', Gtk.FileChooserAction.OPEN,
                                            ['*.phmitheme', '*.json', '*.zip'])
                if not filename: return
                path = Path(filename)
                maximum = 8 * 1024 * 1024 if path.suffix.lower() == '.zip' else MAX_BYTES
                if path.stat().st_size > maximum: raise ValueError('Theme file exceeds its size limit.')
                if path.suffix.lower() == '.zip':
                    project = import_bundle(path.read_bytes(), self.artwork_dir)
                else:
                    project = json.loads(path.read_text(encoding='utf-8'))
                    if not isinstance(project, dict): raise ValueError('Invalid theme document.')
                    if 'background' in project: raise ValueError('Import an artwork theme using its complete ZIP bundle.')
                self.callback('ThemeMakerProjectLoaded', {'project': project})
                return
            filename, text = export_request(raw)
            project = json.loads(text)
            if project.get('schema_version') == 3:
                data = bundle(text, self.artwork_dir)
                destination = self.choose_file('Export theme and artwork bundle', Gtk.FileChooserAction.SAVE,
                                               ['*.zip'], filename.removesuffix('.phmitheme') + '-theme.zip')
                if destination:
                    atomic_save_bytes(destination, data)
                    self.notify('Saved ' + Path(destination).name + '. Extract it to the root of the panel SD card.')
                else: self.notify('Export cancelled. Your draft is unchanged.')
            else:
                destination = self.choose_file('Export PrinterHMI theme', Gtk.FileChooserAction.SAVE,
                                               ['*.phmitheme'], filename)
                if destination:
                    atomic_save(destination, text)
                    self.notify('Saved ' + Path(destination).name + '. Copy it to /PrinterHMI/themes/ on your SD card.')
                else: self.notify('Export cancelled. Your draft is unchanged.')
        except Exception as error:
            if request.get('action') == 'native-ui':
                if self.native: self.native.close(); self.native = None
                self.callback('ThemeMakerNativeError', {'token': request.get('token'), 'message': str(error)})
            elif request.get('action') == 'preview-artwork':
                self.callback('ThemeMakerArtworkError', {'key': request.get('key'), 'message': str(error)})
            else:
                self.notify('Could not complete the action: ' + str(error), True)
        finally:
            self.export_busy = False

    def error(self, text):
        dialog = Gtk.MessageDialog(transient_for=self.window, modal=True,
                                   message_type=Gtk.MessageType.ERROR, buttons=Gtk.ButtonsType.CLOSE,
                                   text='Theme Maker could not display the editor.')
        dialog.format_secondary_text(text)
        dialog.run()
        dialog.destroy()

    def load_failed(self, view, event, uri, error):
        self.error(error.message)
        return True

    def process_terminated(self, view, reason):
        self.error('The interface process stopped. Close and reopen Theme Maker. Exported theme files are unchanged.')


if __name__ == '__main__':
    if not (BASE / 'index.html').is_file():
        sys.exit('The bundled editor files are missing. Reinstall PrinterHMI Theme Maker.')
    raise SystemExit(ThemeMaker().run(sys.argv))
