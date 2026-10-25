#define main legacy_test_main
#include "gcode_viewer_loader_test.c"
#undef main
#include "mesh_preview.h"
static gcode_toolpath_t *overview;
static unsigned refined_count,refined_errors;
static bool loaded(gcode_toolpath_t *m,const char *error){assert(m&&!error&&m->source_hash_valid&&m->extrusion_seen);overview=m;return true;}
static bool refined(struct mesh_preview *mesh,const char *error){refined_count++;if(error){assert(!mesh);refined_errors++;return false;}assert(mesh&&mesh->triangles&&mesh->cell_mm>0);mesh_preview_free(mesh,test_free);return true;}
static void execute(void){assert(task);void(*f)(void*)=task;void *p=job;task=NULL;job=NULL;f(p);assert(!locked&&!gcode_viewer_loader_running());offset=reads=0;}
int main(void){
 assert(gcode_viewer_loader_start("file.gcode",loaded));execute();assert(overview&&!overview->cache);
 unsigned before=requests;assert(gcode_viewer_loader_refine("file.gcode",overview,refined));execute();assert(refined_count==1&&!refined_errors&&requests==before+1);
 const char *original=body;body="G90\nM83\nG1 X10 Y10 Z.2\nG1 X20 E1\nG1 Y21 E1\n";assert(gcode_viewer_loader_refine("file.gcode",overview,refined));execute();assert(refined_errors==1);body=original;
 unsigned prior=refined_count;assert(gcode_viewer_loader_refine("file.gcode",overview,refined));gcode_viewer_loader_cancel();execute();assert(refined_count==prior);
 assert(gcode_viewer_loader_refine("file.gcode",overview,refined));generation++;execute();assert(refined_count==prior);generation--;
 exclusive=true;assert(gcode_viewer_loader_refine("file.gcode",overview,refined));execute();exclusive=false;assert(refined_errors==2);
 fail=3;assert(!gcode_viewer_loader_refine("file.gcode",overview,refined));fail=0;
 gcode_viewer_loader_free(overview);assert(!allocations);puts("PASS: no-SD automatic replay, endpoint snapshots, unchanged-source fingerprint, changed-file rejection, request/profile/exclusive/task cleanup and original preview lifetime");return 0;
}
