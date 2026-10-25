#define main legacy_test_main
#include "gcode_viewer_loader_test.c"
#undef main
#include "mesh_preview.h"
#include "gcode_cache.h"
#include "gcode_surface.h"
static gcode_toolpath_t *overview,*detail;
static bool detail_callback(gcode_toolpath_t *m,const char *error){callbacks++;if(error){errors++;return false;}assert(m&&m->cached_layer==1&&m->count==3&&m->sample_stride==1&&!m->surface&&!m->cache);detail=m;return true;}
static bool overview_callback(gcode_toolpath_t *m,const char *error){assert(!error&&m&&m->cache&&m->cache->complete&&gcode_surface_usable(m->surface)&&m->capacity==32768);overview=m;return true;}
static bool refined_callback(struct mesh_preview *mesh,const char *error){assert(mesh&&!error);mesh_preview_free(mesh,test_free);return true;}
static void execute(void){assert(task);void (*f)(void*)=task;void *p=job;task=NULL;job=NULL;f(p);assert(!locked&&!gcode_viewer_loader_running());offset=reads=0;}
int main(void){
 body="G90\nM83\nG1 X0 Y0 Z.2\nG1 X10 E1\nG1 Y10 E1\nG0 X0 Y0 Z.4\nG1 X10 E1\nG1 Y10 E1\n";
 assert(gcode_viewer_loader_start("file.gcode",overview_callback));execute();assert(overview&&overview->cache->layers==2);
 unsigned before_refine=requests;assert(gcode_viewer_loader_refine("file.gcode",overview,refined_callback));execute();assert(requests==before_refine&&overview->cache->refs==1);
 char filename[128];strcpy(filename,overview->cache->file);unsigned http_requests=requests;
 assert(gcode_viewer_loader_detail(overview,1,detail_callback));assert(!gcode_viewer_loader_detail(overview,0,detail_callback));
 execute();assert(detail&&detail->segments[0].layer==1&&!detail->segments[0].extrusion&&detail->capacity==32768&&requests==http_requests);
 gcode_viewer_loader_free(detail);detail=NULL;
 /* An active SD worker owns a reference after the UI closes its overview. */
 assert(gcode_viewer_loader_detail(overview,1,detail_callback));assert(overview->cache->refs==2);
 gcode_viewer_loader_free(overview);overview=NULL;gcode_viewer_loader_cancel();execute();assert(!allocations&&!fopen(filename,"rb"));
 assert(gcode_viewer_loader_start("file.gcode",overview_callback));execute();assert(overview);
 fail=3;assert(!gcode_viewer_loader_detail(overview,1,detail_callback)&&overview->cache->refs==1);fail=0;
 unsigned before=callbacks;assert(gcode_viewer_loader_detail(overview,1,detail_callback));generation++;execute();assert(callbacks==before);generation--;
 unsigned prior_errors=errors;remove(overview->cache->file);assert(gcode_viewer_loader_detail(overview,1,detail_callback));execute();assert(errors==prior_errors+1);
 gcode_viewer_loader_free(overview);overview=NULL;assert(!allocations);
 assert(gcode_viewer_loader_start("file.gcode",overview_callback));execute();
 assert(gcode_viewer_loader_detail(overview,1,detail_callback));display_allowed=false;execute();display_allowed=true;gcode_viewer_loader_free(overview);assert(!allocations);
 puts("PASS: actual SD-backed file worker and layer worker, compact RAM budget, no HTTP for details, full travel ownership, single flight, worker-held cache lifetime after UI close, task/profile/SD/display failures and complete cleanup");
}
