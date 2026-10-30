# Live appearance changes

Appearance preferences are changed through Settings. The application rebuilds the shell, persistent Dashboard/Camera surfaces and Settings so old local widget styles cannot survive a theme change. Other operator pages are destroyed when leaving them and created with current preferences when reopened. This pass preserves that lifecycle rather than trying to change individual local styles in an unrelated input editor.

Settings now captures its visible section title and offset before an appearance/theme rebuild and restores that section afterward. It stores values, not pointers to deleted widgets. This keeps the operator near Display even when Studio reorders the cards or density changes their heights. A clamped scroll fraction provides a fallback if a section cannot be found.

Accent, density and accessibility dialogs refresh and remain open after successful changes. Their rows and labels are rebuilt using current fonts, colors and metrics, selected/toggled values are retained, and their list scroll position is restored. Close remains fixed below the scrolling options. Popup width and height fit smaller viewports, and option labels wrap without ellipsizing. The built-in/custom theme chooser retains its existing close-after-selection behavior.

Appearance dialogs use the chooser's screen-local opaque modal model, avoiding compositing the underlying Settings page for list scrolling. External deletion clears owner pointers. Rapid taps cannot queue multiple changes. A safe asynchronous rebuild is reserved before persisting a preference; a failed reservation does not save a change, and failed persistence cancels the reservation. Closing a dialog before the callback runs prevents it from being reopened. Requests to open another appearance dialog are ignored while a change is queued, preventing that new dialog from being closed by the older callback.

## Validation

`live_appearance_test.py` exercises real LVGL objects and events in all five themes, all densities, both text sizes and 1024x600, 640x480 and 480x320 viewports. It checks refreshed label fonts/colors, option state, bounded bodies and pinned Close, background draw suppression while scrolling, duplicate taps, no-op selections, save/queue failures, external deletion, queued-close behavior and object/timer cleanup. It tests section reorder restoration and executes the Settings rebuild function extracted directly from its source with a replacement owner.

Existing theme chooser/custom theme lifecycle and action-text/pressed-frame audits pass. Strict host compilation covers Settings and appearance owners. Target firmware compilation, physical theme transition flashes, touch behavior, dropdowns/keyboards after transitions and long-running printer operation remain panel checks. Previously pending physical print/recovery tests remain pending.




Validation statements above record the implementation-stage checks. Current
release status and open limitations are maintained in [Known issues](KNOWN_ISSUES.md),
the [changelog](../CHANGELOG.md) and the [current audit](PROJECT_AUDIT_2026_10_30.md).
