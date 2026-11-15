#!/usr/bin/env python3
"""Check preparation-layer regression against the production resolver."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
cli = argparse.ArgumentParser(description=__doc__)
cli.add_argument('--source', type=Path, default=root/'main/printer_layer_resolver.c')
args = cli.parse_args()
source = args.source.read_text()
legacy = 'const printer_layer_metadata_t *metadata' not in source
if legacy:
    started = ',started' if 'bool print_started)' in source else ''
    source = source.replace('printer_layer_resolver_resolve(', 'legacy_resolve(', 1)
    source += """
printer_layer_result_t printer_layer_resolver_resolve(int c,int t,double z,
    const printer_layer_metadata_t *m,double p,bool started) {
    (void)started;return legacy_resolve(c,t,z,m ? m->object_height : 0,
        m ? m->layer_height : 0,p""" + started + """);
}
"""
fixture = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <float.h>
static printer_layer_result_t simple_resolve(int c,int t,double z,
    double height,double layer,double progress,bool started) {
    printer_layer_metadata_t metadata={.object_height=height,.layer_height=layer};
    return printer_layer_resolver_resolve(c,t,z,&metadata,progress,started);
}
int main(void) {
    const double heights[]={0,.2,4,8,20,40,50,100,-1,NAN,INFINITY};
    const double progresses[]={0,.0001,.04,.5,1,NAN,-1};
    for(unsigned i=0;i<sizeof(heights)/sizeof(*heights);i++)
        for(unsigned j=0;j<sizeof(progresses)/sizeof(*progresses);j++)
            for(int report=-1;report<=0;report++) {
                printer_layer_result_t r=simple_resolve(
                    report,40,heights[i],20,.5,progresses[j],false);
                assert(r.valid&&r.current==0&&r.total==40);
                r=simple_resolve(report,-1,heights[i],20,.5,progresses[j],false);
                assert(r.valid&&r.current==0&&r.total==40);
            }
    printer_layer_result_t r=simple_resolve(7,40,20,20,.5,.04,false);
    assert(r.valid&&r.current==7); /* Explicit reports remain authoritative. */
    r=simple_resolve(0,40,6,20,.5,.4,true);
    assert(r.valid&&r.current==12); /* External starts still get a fallback. */
    r=simple_resolve(-1,40,-1,20,.5,.5,true);
    assert(r.valid&&r.current==20);
    r=simple_resolve(-1,40,-1,20,.5,NAN,true);
    assert(!r.valid&&r.current==-1&&r.total==40);
    r=simple_resolve(-1,-1,-1,20,.5,-1,true);
    assert(!r.valid&&r.current==-1&&r.total==40);
    r=simple_resolve(-1,40,-1,20,.5,0,true);
    assert(r.valid&&r.current==1);
    r=simple_resolve(-1,-1,50,0,0,.5,false);
    assert(!r.valid&&r.current==0);
    r=simple_resolve(0,40,6,20,.5,0,true);
    assert(r.valid&&r.current==12);
    r=simple_resolve(0,40,6,20,.5,NAN,true);
    assert(r.valid&&r.current==12);
    r=simple_resolve(0,40,-1,20,.5,0,true);
    assert(r.valid&&r.current==0);
    printer_layer_metadata_t metadata={.object_height=10,.layer_height=.2,.first_layer_height=.4};
    r=printer_layer_resolver_resolve(-1,-1,.6,&metadata,.2,true);
    assert(r.valid&&r.current==2&&r.total==49);
    metadata.total_layers=57;
    r=printer_layer_resolver_resolve(-1,-1,.6,&metadata,.2,true);
    assert(r.current==2&&r.total==57);
    r=printer_layer_resolver_resolve(7,99,100,&metadata,.1,false);
    assert(r.current==7&&r.total==99);
    r=printer_layer_resolver_resolve(58,-1,.6,&metadata,.8,true);
    assert(!r.valid&&r.current==58&&r.total==-1);
    r=printer_layer_resolver_resolve(58,60,.6,&metadata,.8,true);
    assert(r.valid&&r.current==58&&r.total==60);
    metadata.total_layers=0;metadata.object_height=10.01;metadata.first_layer_height=.2;
    r=printer_layer_resolver_resolve(-1,-1,.2,&metadata,.1,true);
    assert(r.current==1&&r.total==51);
    metadata.object_height=.1;metadata.first_layer_height=.4;
    r=printer_layer_resolver_resolve(-1,-1,.1,&metadata,.1,true);
    assert(r.current==1&&r.total==1);
    metadata.object_height=DBL_MAX;metadata.layer_height=DBL_MIN;
    r=printer_layer_resolver_resolve(-1,-1,.1,&metadata,.1,true);assert(!r.valid);
    r=printer_layer_resolver_resolve(2,40,NAN,NULL,.1,true);assert(r.current==2&&r.total==40);
#ifndef LEGACY_RESOLVER
    assert(!printer_layer_resolver_print_started(0,true,0));
    assert(!printer_layer_resolver_print_started(30,true,0));
    assert(!printer_layer_resolver_print_started(0,true,12));
    assert(!printer_layer_resolver_print_started(30,true,NAN));
    assert(!printer_layer_resolver_print_started(30,true,INFINITY));
    assert(!printer_layer_resolver_print_started(30,true,-1));
    assert(!printer_layer_resolver_print_started(INFINITY,true,12));
    assert(!printer_layer_resolver_print_started(NAN,true,12));
    assert(printer_layer_resolver_print_started(30,true,12));
    assert(printer_layer_resolver_print_started(30,false,0));
    assert(!printer_layer_resolver_print_started(0,false,0));
#endif
    puts("PASS: raised/probing Z and file progress cannot advance preparation layers; reported/metadata count priority, first-layer thickness, partial last layer, active-print fallback and numeric boundaries");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='print-layers-') as directory:
    tmp = Path(directory)
    (tmp/'test.c').write_text(source+'\n'+fixture)
    exe = tmp/'test'
    subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    *(['-DLEGACY_RESOLVER'] if legacy else []),
                    '-I',str(root/'main'),str(tmp/'test.c'),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
