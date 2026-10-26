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
 strcpy(api,key_marker);verify_key_erasure=true;
 switch_during_endpoint=true;assert(!gcode_viewer_loader_start("file.gcode",loaded));generation--;assert(!allocations&&!requests);
 fail=3;assert(!gcode_viewer_loader_start("file.gcode",loaded));fail=0;assert(!allocations);
 assert(gcode_viewer_loader_start("file.gcode",loaded));execute();assert(overview&&!overview->cache);
 switch_during_endpoint=true;assert(!gcode_viewer_loader_refine("file.gcode",overview,refined));generation--;assert(!gcode_viewer_loader_running());
 unsigned before=requests,waits=delays;
 assert(gcode_viewer_loader_refine("file.gcode",overview,refined));execute();assert(refined_count==1&&!refined_errors&&requests==before&&delays-waits==1);
 /* Optional private-copy OOM retains the fingerprint-checked HTTP fallback. */
 fail_replay_copy=true;assert(gcode_viewer_loader_refine("file.gcode",overview,refined));execute();fail_replay_copy=false;assert(refined_count==2&&!refined_errors&&requests==before+1);
 /* Reduced RAM samples must never replace a complete HTTP source replay. */
 overview->sample_stride=2;before=requests;assert(gcode_viewer_loader_refine("file.gcode",overview,refined));execute();assert(refined_count==3&&!refined_errors&&requests==before+1);
 const char *original=body;body="G90\nM83\nG1 X10 Y10 Z.2\nG1 X20 E1\nG1 Y21 E1\n";assert(gcode_viewer_loader_refine("file.gcode",overview,refined));execute();assert(refined_errors==1);body=original;
 unsigned prior=refined_count;assert(gcode_viewer_loader_refine("file.gcode",overview,refined));gcode_viewer_loader_cancel();execute();assert(refined_count==prior);
 assert(gcode_viewer_loader_refine("file.gcode",overview,refined));generation++;execute();assert(refined_count==prior);generation--;
 exclusive=true;assert(gcode_viewer_loader_refine("file.gcode",overview,refined));execute();exclusive=false;assert(refined_errors==2);
 fail=3;assert(!gcode_viewer_loader_refine("file.gcode",overview,refined));fail=0;
 gcode_viewer_loader_free(overview);overview=NULL;assert(!allocations);
 /* The private RAM source remains valid after its UI overview is retired. */
 assert(gcode_viewer_loader_start("file.gcode",loaded));execute();before=requests;assert(gcode_viewer_loader_refine("file.gcode",overview,refined));gcode_viewer_loader_free(overview);overview=NULL;execute();assert(requests==before&&!allocations);
 /* A cancellation arriving during a timed yield must be observed immediately. */
 assert(gcode_viewer_loader_start("file.gcode",loaded));execute();prior=refined_count;assert(gcode_viewer_loader_refine("file.gcode",overview,refined));cancel_during_delay=true;execute();cancel_during_delay=false;assert(refined_count==prior);gcode_viewer_loader_free(overview);overview=NULL;assert(!allocations);
 /* Cancelled source copies and failed task starts must also release storage. */
 assert(gcode_viewer_loader_start("file.gcode",loaded));execute();assert(gcode_viewer_loader_refine("file.gcode",overview,refined));gcode_viewer_loader_free(overview);overview=NULL;gcode_viewer_loader_cancel();execute();assert(!allocations);
 assert(gcode_viewer_loader_start("file.gcode",loaded));execute();fail=3;assert(!gcode_viewer_loader_refine("file.gcode",overview,refined));fail=0;gcode_viewer_loader_free(overview);assert(!allocations);
 puts("PASS: endpoint/profile capture fence and pre-free key erasure, owned complete-RAM refinement, rate-limited yielding, copy-OOM/reduced-RAM HTTP fallback, retirement/cancellation/task cleanup, no-SD automatic replay, endpoint snapshots, unchanged-source fingerprint, changed-file rejection, request/profile/exclusive/task cleanup and original preview lifetime");return 0;
}
