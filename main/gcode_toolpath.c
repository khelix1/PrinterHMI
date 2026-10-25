#include "gcode_toolpath.h"
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void gcode_parser_init(gcode_parser_t *p,gcode_toolpath_t *m,gcode_segment_t *storage,size_t capacity)
{
    memset(p,0,sizeof(*p));memset(m,0,sizeof(*m));p->path=m;m->segments=storage;
    p->line_number=1;p->units=1;m->sample_stride=1;m->cached_layer=-1;p->extrusion_width=.45f;p->extrusion_height=.2f;
    m->capacity=capacity<GCODE_TOOLPATH_SEGMENTS_MAX?capacity:GCODE_TOOLPATH_SEGMENTS_MAX;
}
static bool stopped(const gcode_parser_t *p){return p->halted||p->path->bounded||p->path->malformed;}
/* G-code E is an axis word, never a floating-point exponent. */
static double number(char *s,char **end)
{
    char *p=s;if(*p=='+'||*p=='-')p++;
    bool digit=false;while(isdigit((unsigned char)*p)){digit=true;p++;}
    if(*p=='.'){p++;while(isdigit((unsigned char)*p)){digit=true;p++;}}
    if(!digit){*end=s;return 0;}
    char saved=*p;*p=0;double value=strtod(s,NULL);*p=saved;*end=p;return value;
}
static void bounds(gcode_toolpath_t *m,gcode_point_t v)
{
    m->min.x=fminf(m->min.x,v.x);m->max.x=fmaxf(m->max.x,v.x);
    m->min.y=fminf(m->min.y,v.y);m->max.y=fmaxf(m->max.y,v.y);
    m->min.z=fminf(m->min.z,v.z);m->max.z=fmaxf(m->max.z,v.z);
}
/* Store only a normalized command name, never user-supplied parameters. */
static const char *command_name(const char *s,char out[48])
{
    while(isspace((unsigned char)*s))s++;
    size_t n=0;
    bool numeric=(toupper((unsigned char)s[0])=='G'||toupper((unsigned char)s[0])=='M'||toupper((unsigned char)s[0])=='T')&&isdigit((unsigned char)s[1]);
    if(numeric){out[n++]=(char)toupper((unsigned char)*s++);while(isdigit((unsigned char)*s)||*s=='.'){if(n<47)out[n++]=*s;s++;}}
    else while(isalnum((unsigned char)*s)||*s=='_'){if(n<47)out[n++]=(char)toupper((unsigned char)*s);s++;}
    out[n]=0;return s;
}
static void diagnostic(gcode_parser_t *p,const char *reason)
{
    gcode_toolpath_t *m=p->path;if(m->stop_reason[0])return;
    m->stop_line=p->line_number;m->lines=p->line_number;
    snprintf(m->stop_reason,sizeof(m->stop_reason),"%s",reason);
    command_name(p->line,m->stop_command);
}
void gcode_parser_limit(gcode_parser_t *p,const char *reason){p->line[p->used]=0;p->path->bounded=true;diagnostic(p,reason);}
static void invalid(gcode_parser_t *p){p->path->malformed=true;diagnostic(p,"Invalid input");}
static void unsupported(gcode_parser_t *p){p->path->unsupported=true;p->halted=true;diagnostic(p,"Unsupported command");}
static bool bookkeeping(const char *name)
{
    static const char *const names[]={"SET_PRINT_STATS_INFO","EXCLUDE_OBJECT_DEFINE","EXCLUDE_OBJECT_START","EXCLUDE_OBJECT_END",
        "SET_VELOCITY_LIMIT","SET_PRESSURE_ADVANCE","SET_FAN_SPEED","SET_HEATER_TEMPERATURE","TEMPERATURE_WAIT","RESPOND","GET_POSITION","STATUS","SET_RETRACTION"};
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);i++)if(!strcmp(name,names[i]))return true;
    return false;
}
static bool ignored_m(int code)
{
    static const int codes[]={104,109,140,190,106,107,204,205,220,221,73,117,400,84,201,203,900,486,105,114,115,118};
    for(size_t i=0;i<sizeof(codes)/sizeof(codes[0]);i++)if(code==codes[i])return true;
    return false;
}
static bool valid_point(gcode_point_t a){return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z)&&fabsf(a.x)<=1000000&&fabsf(a.y)<=1000000&&fabsf(a.z)<=1000000;}
/* Keep independent, uniformly spaced samples across the entire input. Never
 * connect discarded endpoints into invented chords. Layer/tool anchors and the
 * final segment retain sparse later features even after repeated compaction. */
static void retain(gcode_toolpath_t *m,gcode_segment_t segment)
{
    if(segment.sequence%m->sample_stride)return;
    while(m->count==m->capacity){
        uint32_t stride=m->sample_stride>UINT32_MAX/2?UINT32_MAX:m->sample_stride*2;
        size_t kept=0;for(size_t i=0;i<m->count;i++)if(m->segments[i].sequence%stride==0)m->segments[kept++]=m->segments[i];
        m->count=kept;m->sample_stride=stride;
        if(segment.sequence%stride)return;
    }
    m->segments[m->count++]=segment;
}
static bool add_segment(gcode_parser_t *p,gcode_point_t a,gcode_point_t b,bool extrusion)
{
    gcode_toolpath_t *m=p->path;
    if(!valid_point(a)||!valid_point(b)){invalid(p);return false;}
    if(fabsf(a.x-b.x)<.000001f&&fabsf(a.y-b.y)<.000001f&&fabsf(a.z-b.z)<.000001f)return true;
    if(!m->capacity||!m->segments){gcode_parser_limit(p,"No geometry storage");return false;}
    if(m->input_segments==UINT32_MAX){gcode_parser_limit(p,"Path count limit");return false;}
    bool new_layer=extrusion&&(!m->layer_count||fabsf(b.z-m->layer_z[m->layer_count-1])>.001f);
    if(new_layer){
        if(m->layer_count==GCODE_TOOLPATH_LAYERS_MAX){gcode_parser_limit(p,"Layer limit");return false;}
        m->layer_z[m->layer_count++]=b.z;
        for(size_t i=m->count;i>0;i--){
            gcode_segment_t *travel=&m->segments[i-1];
            if(travel->extrusion||(m->extrusion_seen&&travel->sequence<=m->last_extrusion_sequence))break;
            travel->layer=(uint16_t)(m->layer_count-1);
        }
    }
    if(!m->input_segments)m->min=m->max=a;
    bounds(m,a);bounds(m,b);m->tool_mask|=(uint16_t)(1u<<p->tool);
    gcode_segment_t segment={.a=a,.b=b,.layer=m->layer_count?(uint16_t)(m->layer_count-1):0,.extrusion=extrusion,.tool=p->tool,.sequence=m->input_segments++,.width=p->extrusion_width,.height=p->extrusion_height};
    if(new_layer)m->layer_anchor[m->layer_count-1]=segment;
    if(extrusion){
        if(!m->extrusion_seen)m->extrusion_min=m->extrusion_max=a;
        gcode_point_t points[2]={a,b};for(unsigned i=0;i<2;i++){
            m->extrusion_min.x=fminf(m->extrusion_min.x,points[i].x);m->extrusion_min.y=fminf(m->extrusion_min.y,points[i].y);m->extrusion_min.z=fminf(m->extrusion_min.z,points[i].z);
            m->extrusion_max.x=fmaxf(m->extrusion_max.x,points[i].x);m->extrusion_max.y=fmaxf(m->extrusion_max.y,points[i].y);m->extrusion_max.z=fmaxf(m->extrusion_max.z,points[i].z);
        }
        m->max_width=fmaxf(m->max_width,segment.width);m->max_height=fmaxf(m->max_height,segment.height);
        m->extrusion_seen=true;m->last_extrusion_sequence=segment.sequence;
        if(!(m->tool_anchor_mask&(1u<<p->tool))){m->tool_anchor[p->tool]=segment;m->tool_anchor_mask|=(uint16_t)(1u<<p->tool);}
    }
    m->last_segment=segment;retain(m,segment);if(p->segment_sink)p->segment_sink(p->sink_context,&segment);return true;
}
static double sweep(double ax,double ay,double bx,double by,bool clockwise)
{
    double angle=atan2(ax*by-ay*bx,ax*bx+ay*by);
    if(clockwise){if(angle>=0)angle-=6.283185307179586;}
    else if(angle<=0)angle+=6.283185307179586;
    return angle;
}
static void arc(gcode_parser_t *p,gcode_point_t a,gcode_point_t b,const double *v,unsigned mask,bool clockwise,bool extrusion)
{
    if(p->known_xy!=3){unsupported(p);snprintf(p->path->stop_reason,48,"Arc start not established");return;}
    int u=p->plane==2?1:0,w=p->plane==0?1:2,h=3-u-w;
    double start[3]={a.x,a.y,a.z},end[3]={b.x,b.y,b.z},base[3]={p->offset.x,p->offset.y,p->offset.z};
    double cx,cy,radius,angle;unsigned centers=(1u<<(u+4))|(1u<<(w+4));
    if(mask&128){
        if(mask&112){invalid(p);return;}
        double r=v[7]*p->units,dx=end[u]-start[u],dy=end[w]-start[w],d=hypot(dx,dy);
        radius=fabs(r);if(d<.000001||radius<d*.5-.000001||radius<.000001){invalid(p);return;}
        double height=sqrt(fmax(0,radius*radius-d*d*.25));bool found=false;cx=cy=angle=0;
        for(int sign=-1;sign<=1;sign+=2){
            double x=(start[u]+end[u])*.5-sign*dy/d*height,y=(start[w]+end[w])*.5+sign*dx/d*height;
            double turn=sweep(start[u]-x,start[w]-y,end[u]-x,end[w]-y,clockwise);
            if((r>=0&&fabs(turn)<=3.141592653589793+1e-9)||(r<0&&fabs(turn)>=3.141592653589793-1e-9)){cx=x;cy=y;angle=turn;found=true;break;}
        }if(!found){invalid(p);return;}
    }else{
        if(!(mask&centers)||(mask&112&~centers)){invalid(p);return;}
        cx=p->absolute_center?((mask&(1u<<(u+4)))?v[u+4]*p->units+base[u]:start[u]):start[u]+v[u+4]*p->units;
        cy=p->absolute_center?((mask&(1u<<(w+4)))?v[w+4]*p->units+base[w]:start[w]):start[w]+v[w+4]*p->units;
        radius=hypot(start[u]-cx,start[w]-cy);
        if(radius<.000001||fabs(hypot(end[u]-cx,end[w]-cy)-radius)>fmax(.02,radius*.001)){invalid(p);return;}
        angle=sweep(start[u]-cx,start[w]-cy,end[u]-cx,end[w]-cy,clockwise);
    }
    double length=hypot(radius*angle,end[h]-start[h]);
    double required=fmax(1,fmax(ceil(length),ceil(fabs(angle)/.08726646259971647)));
    if(!isfinite(required)){invalid(p);return;}
    if(required>GCODE_TOOLPATH_ARC_MAX){required=GCODE_TOOLPATH_ARC_MAX;p->path->coarse_arcs=true;}
    unsigned n=(unsigned)required;p->path->arcs++;double theta=atan2(start[w]-cy,start[u]-cx);gcode_point_t previous=a;
    for(unsigned i=1;i<=n;i++){
        double at=(double)i/n,point[3];point[u]=cx+radius*cos(theta+angle*at);point[w]=cy+radius*sin(theta+angle*at);point[h]=start[h]+(end[h]-start[h])*at;
        gcode_point_t next=i==n?b:(gcode_point_t){(float)point[0],(float)point[1],(float)point[2]};
        if(!add_segment(p,previous,next,extrusion))return;
        previous=next;
    }
}
static void change_tool(gcode_parser_t *p,unsigned tool)
{
    if(tool>=GCODE_TOOLPATH_TOOLS_MAX){unsupported(p);snprintf(p->path->stop_reason,48,"Tool index exceeds 15");return;}
    p->tool_e[p->tool]=p->e;p->tool=(uint8_t)tool;p->e=p->tool_e[tool];p->known_xy=0;
    p->path->unsupported=true;p->path->tool_changes++;
    if(!p->path->tool_line)p->path->tool_line=p->line_number;
}
/* Bounded NAME=value reader for explicit Klipper state/offset commands. */
static bool parameter(const char *text,const char *name,char out[64])
{
    bool found=false;
    while(*text){
        while(isspace((unsigned char)*text))text++;
        const char *start=text;while(*text&&!isspace((unsigned char)*text))text++;
        size_t key=strlen(name),n=(size_t)(text-start);
        if(n>key&&start[key]=='='){
            bool same=true;for(size_t i=0;i<key;i++)if(toupper((unsigned char)start[i])!=name[i])same=false;
            if(same){
                if(found||n-key-1>=64){memset(out,'!',63);out[63]=0;return true;}
                memcpy(out,start+key+1,n-key-1);out[n-key-1]=0;found=true;
            }
        }
    }return found;
}
static bool numeric_parameter(gcode_parser_t *p,const char *text,const char *key,double *value,bool *present)
{
    char raw[64];*present=parameter(text,key,raw);if(!*present)return true;
    char *end;double v=number(raw,&end);if(*end||end==raw||!isfinite(v)||fabs(v)>1000000){invalid(p);return false;}*value=v;return true;
}
static bool state_command(gcode_parser_t *p,const char *name,const char *text)
{
    if(!strcmp(name,"ACTIVATE_EXTRUDER")){
        char tool[64];if(!parameter(text,"EXTRUDER",tool)){invalid(p);return true;}
        if(!strcmp(tool,"extruder")){change_tool(p,0);return true;}
        if(strncmp(tool,"extruder",8)||strlen(tool)<=8||!isdigit((unsigned char)tool[8])){unsupported(p);return true;}
        char *end;unsigned long n=strtoul(tool+8,&end,10);
        if(*end||n>=GCODE_TOOLPATH_TOOLS_MAX){unsupported(p);return true;}change_tool(p,(unsigned)n);return true;
    }
    bool offset=!strcmp(name,"SET_GCODE_OFFSET"),save=!strcmp(name,"SAVE_GCODE_STATE"),restore=!strcmp(name,"RESTORE_GCODE_STATE");
    if(!offset&&!save&&!restore)return false;
    double move=0;bool exists;if(!numeric_parameter(p,text,"MOVE",&move,&exists))return true;
    if(move!=0&&move!=1){invalid(p);return true;}
    if(offset){
        double delta[4]={0};const char *keys[]={"X","Y","Z","E"};
        for(int i=0;i<4;i++){
            double value=0;bool absolute;if(!numeric_parameter(p,text,keys[i],&value,&absolute))return true;
            if(!absolute){char adjust[16];snprintf(adjust,sizeof(adjust),"%s_ADJUST",keys[i]);bool found;if(!numeric_parameter(p,text,adjust,&value,&found))return true;if(!found)continue;value+=p->home[i];}
            delta[i]=value-p->home[i];p->home[i]=value;
        }
        gcode_point_t a=p->position,b=a;
        p->offset.x+=(float)delta[0];p->offset.y+=(float)delta[1];p->offset.z+=(float)delta[2];
        if(move){b.x+=(float)delta[0];b.y+=(float)delta[1];b.z+=(float)delta[2];if(p->known_xy==3)add_segment(p,a,b,false);p->position=b;}
        else p->e-=delta[3];
        if(!valid_point(p->offset)||!valid_point(p->position)){invalid(p);}
        return true;
    }
    char key[64]="default";parameter(text,"NAME",key);
    for(const char *c=key;*c;c++)if(!isalnum((unsigned char)*c)&&*c!='_'&&*c!='-'&&*c!='.'){unsupported(p);snprintf(p->path->stop_reason,48,"Unsupported saved-state name");return true;}
    if(strlen(key)>31){gcode_parser_limit(p,"Saved-state name limit");return true;}
    gcode_saved_state_t *state=NULL,*empty=NULL;
    for(unsigned i=0;i<GCODE_TOOLPATH_STATES_MAX;i++){if(p->saved[i].used&&!strcmp(p->saved[i].name,key))state=&p->saved[i];if(!p->saved[i].used&&!empty)empty=&p->saved[i];}
    if(save){
        if(!state)state=empty;
        if(!state){gcode_parser_limit(p,"Saved-state slot limit");return true;}
        *state=(gcode_saved_state_t){.used=true,.relative_xyz=p->relative_xyz,.relative_e=p->relative_e,.absolute_center=p->absolute_center,.tool=p->tool,.plane=p->plane,.known_xy=p->known_xy,.position=p->position,.offset=p->offset,.e=p->e,.units=p->units};
        memcpy(state->home,p->home,sizeof(p->home));memcpy(state->name,key,strlen(key)+1);return true;
    }
    if(!state||state->tool!=p->tool){unsupported(p);snprintf(p->path->stop_reason,48,"Missing state or tool mismatch");return true;}
    if(move){if(p->known_xy==3&&state->known_xy==3)add_segment(p,p->position,state->position,false);p->position=state->position;p->known_xy=state->known_xy;}
    p->relative_xyz=state->relative_xyz;p->relative_e=state->relative_e;p->offset=state->offset;p->e=state->e;memcpy(p->home,state->home,sizeof(p->home));return true;
}

static void parse_line(gcode_parser_t *p)
{
    gcode_toolpath_t *m=p->path;m->lines=p->line_number;p->line[p->used]=0;
    char *s=p->line;while(isspace((unsigned char)*s))s++;
    if(!*s||*s==';')return;
    char name[48];const char *tail=command_name(s,name);
    bool extended=!((toupper((unsigned char)s[0])=='G'||toupper((unsigned char)s[0])=='M'||toupper((unsigned char)s[0])=='T')&&isdigit((unsigned char)s[1]));
    if(extended){
        if(*tail&&!isspace((unsigned char)*tail)){unsupported(p);return;}
        if(state_command(p,name,tail))return;
        if(bookkeeping(name))return;
        if(!strcmp(name,"START_PRINT")||!strcmp(name,"END_PRINT")||!strcmp(name,"STOP_PRINT")){
            m->unsupported=true;m->omitted_count++;
            if(!m->omitted_line){m->omitted_line=p->line_number;snprintf(m->omitted_command,sizeof(m->omitted_command),"%s",name);}
            p->known_xy=0;return;
        }
        unsupported(p);return;
    }
    char kind=(char)toupper((unsigned char)*s++);char *end;
    double command=number(s,&end);if(end==s||!isfinite(command)){invalid(p);return;}s=end;
    if(kind=='G'&&(fabs(command-90.1)<.000001||fabs(command-91.1)<.000001)){p->absolute_center=fabs(command-90.1)<.000001;return;}
    if(command<0||command>100000||floor(command)!=command){unsupported(p);return;}
    int code=(int)command;
    if(kind=='M'){
        if(code==82)p->relative_e=false;
        else if(code==83)p->relative_e=true;
        else if(!ignored_m(code))unsupported(p);
        return;
    }
    if(kind=='T'){change_tool(p,(unsigned)code);return;}
    if(code==90){p->relative_xyz=false;return;}
    if(code==91){p->relative_xyz=true;return;}
    if(code==20){p->units=25.4;return;}
    if(code==21){p->units=1;return;}
    if(code==17||code==18||code==19){p->plane=(uint8_t)(code-17);return;}
    if(code==4)return;
    if(code==10||code==11){while(isspace((unsigned char)*s))s++;if(*s){unsupported(p);return;}p->path->retracts++;p->path->unsupported=true;return;}
    if(code==28){
        unsigned axes=0;while(*s){while(isspace((unsigned char)*s))s++;if(!*s)break;char axis=(char)toupper((unsigned char)*s++);const char *at=strchr("XYZ",axis);if(!at){unsupported(p);return;}axes|=1u<<(unsigned)(at-"XYZ");if(*s=='+'||*s=='-'||*s=='.'||isdigit((unsigned char)*s)){(void)number(s,&end);s=end;}}
        if(!axes)axes=7;
        if(axes&1)p->offset.x=(float)p->home[0];
        if(axes&2)p->offset.y=(float)p->home[1];
        if(axes&4)p->offset.z=(float)p->home[2];
        p->known_xy=0;m->unsupported=true;m->homings++;if(!m->homing_line)m->homing_line=p->line_number;return;
    }
    if(code!=0&&code!=1&&code!=2&&code!=3&&code!=92){unsupported(p);return;}
    double v[8]={0};unsigned mask=0;
    while(*s&&*s!=';'&&*s!='*'){
        if(isspace((unsigned char)*s)){s++;continue;}
        char axis=(char)toupper((unsigned char)*s++);
        if(axis=='('){while(*s&&*s!=')')s++;if(*s)s++;continue;}
        double value=number(s,&end);
        if(end==s||!isfinite(value)||fabs(value)>1000000){invalid(p);return;}
        s=end;const char *index=strchr("XYZEIJKR",axis);
        if(index){unsigned i=(unsigned)(index-"XYZEIJKR");if(mask&(1u<<i)){invalid(p);return;}v[i]=value;mask|=1u<<i;}
        else if(axis!='F'){unsupported(p);return;}
    }
    if(code!=2&&code!=3&&(mask&240)){unsupported(p);return;}
    if(code==92){
        if(!(mask&15)){p->offset=p->position;p->e=0;return;}
        if(mask&1)p->offset.x=p->position.x-(float)(v[0]*p->units);
        if(mask&2)p->offset.y=p->position.y-(float)(v[1]*p->units);
        if(mask&4)p->offset.z=p->position.z-(float)(v[2]*p->units);
        if(mask&8)p->e=v[3]*p->units;
        return;
    }
    gcode_point_t a=p->position,b=a;
    if(mask&1)b.x=p->relative_xyz?a.x+(float)(v[0]*p->units):(float)(v[0]*p->units)+p->offset.x;
    if(mask&2)b.y=p->relative_xyz?a.y+(float)(v[1]*p->units):(float)(v[1]*p->units)+p->offset.y;
    if(mask&4)b.z=p->relative_xyz?a.z+(float)(v[2]*p->units):(float)(v[2]*p->units)+p->offset.z;
    if(fabsf(b.x)>1000000||fabsf(b.y)>1000000||fabsf(b.z)>1000000){invalid(p);return;}
    double e=(mask&8)?((p->relative_e||p->relative_xyz)?p->e+v[3]*p->units:v[3]*p->units):p->e;
    bool extrusion=e>p->e+0.000001;
    unsigned known=p->known_xy;
    if(code==2||code==3)arc(p,a,b,v,mask,code==2,extrusion);
    else if(known==3)add_segment(p,a,b,extrusion);
    p->known_xy|=mask&3;p->position=b;p->e=e;
}

static void comment_dimensions(gcode_parser_t *p){
    if(!p->comment||p->comment_overflow)return;
    for(size_t i=0;i<p->used;i++)if(!isspace((unsigned char)p->line[i]))return;
    p->comment_text[p->comment_used]=0;char *s=p->comment_text;while(isspace((unsigned char)*s))s++;
    float *value=NULL;double min=0,max=0;
    if(!strncmp(s,"WIDTH:",6)){s+=6;value=&p->extrusion_width;min=.05;max=10;}
    else if(!strncmp(s,"HEIGHT:",7)){s+=7;value=&p->extrusion_height;min=.01;max=5;}
    if(!value)return;
    char *end;double v=strtod(s,&end);if(end==s||!isfinite(v)||v<min||v>max)return;while(isspace((unsigned char)*end))end++;if(*end)return;
    *value=(float)v;p->path->dimension_tags++;
}
bool gcode_parser_feed(gcode_parser_t *p,const void *data,size_t size)
{
    const unsigned char *bytes=data;
    for(size_t i=0;i<size;i++){
        if(stopped(p))return false;
        if(p->path->bytes==GCODE_TOOLPATH_BYTES_MAX){gcode_parser_limit(p,"64 MiB read limit");return false;}
        p->path->bytes++;unsigned char c=bytes[i];
        if(p->last_cr){p->last_cr=false;if(c=='\n')continue;}
        if(c=='\n'||c=='\r'){
            comment_dimensions(p);parse_line(p);p->line_number++;
            p->used=0;p->line[0]=0;p->dropping=false;p->comment=false;p->comment_used=0;p->comment_overflow=false;p->last_cr=c=='\r';
        }else if(!p->dropping){
            if(!p->comment&&c==';'){p->comment=true;continue;}
            if(p->comment){if(p->comment_used+1<sizeof(p->comment_text))p->comment_text[p->comment_used++]=(char)c;else p->comment_overflow=true;if(c<32&&c!='\t')p->comment_overflow=true;continue;}
            if(c<32&&c!='\t'){p->line[p->used]=0;invalid(p);return false;}
            if(p->used+1>=sizeof(p->line)){
                p->line[p->used]=0;char name[48];const char *tail=command_name(p->line,name);
                /* Object polygons/messages can be long; only exact known
                 * bookkeeping commands may discard the rest of their line. */
                if(bookkeeping(name)&&(!*tail||isspace((unsigned char)*tail))){p->dropping=true;continue;}
                invalid(p);snprintf(p->path->stop_reason,sizeof(p->path->stop_reason),"Command line too long");return false;
            }
            p->line[p->used++]=(char)c;
        }
    }
    return !stopped(p);
}
void gcode_parser_finish(gcode_parser_t *p){if((p->used||p->comment)&&!stopped(p)){comment_dimensions(p);parse_line(p);}p->used=0;p->line[0]=0;}

void gcode_view_reset(gcode_view_t *v){*v=(gcode_view_t){.yaw=-.7f,.pitch=.7f,.zoom=1,.layer=-1};}
static void line(uint16_t *pixels,int w,int h,int x0,int y0,int x1,int y1,uint16_t color)
{
    float ax=x0,ay=y0,vx=(float)x1-x0,vy=(float)y1-y0,lo=0,hi=1,planes[4]={-vx,vx,-vy,vy},distance[4]={ax,w-1-ax,ay,h-1-ay};
    for(unsigned i=0;i<4;i++){if(fabsf(planes[i])<.00001f){if(distance[i]<0)return;continue;}float t=distance[i]/planes[i];if(planes[i]<0)lo=fmaxf(lo,t);else hi=fminf(hi,t);if(lo>hi)return;}
    x0=(int)lroundf(fmaxf(0,fminf(w-1,ax+lo*vx)));y0=(int)lroundf(fmaxf(0,fminf(h-1,ay+lo*vy)));x1=(int)lroundf(fmaxf(0,fminf(w-1,ax+hi*vx)));y1=(int)lroundf(fmaxf(0,fminf(h-1,ay+hi*vy)));
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;){if(x0>=0&&y0>=0&&x0<w&&y0<h)pixels[(size_t)y0*w+x0]=color;if(x0==x1&&y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}
}
/* Scan only rows/columns intersecting the capsule, not its entire bounding
 * box. Depth is interpolated along the path and curved across the bead. */
void gcode_toolpath_raster_bead(uint16_t *pixels,float *depth,int w,int h,float ax,float ay,float bx,float by,
                 float za,float zb,float radius,float depth_radius,uint16_t color,bool shaded)
{
    float dx=bx-ax,dy=by-ay,len2=dx*dx+dy*dy,r2=radius*radius;
    int y0=(int)floorf(fminf(ay,by)-radius),y1=(int)ceilf(fmaxf(ay,by)+radius);
    if(y0<0)y0=0;
    if(y1>=h)y1=h-1;
    for(int y=y0;y<=y1;y++){
        float lo=0,hi=1;
        if(fabsf(dy)>.0001f){
            float ta=(y-radius-ay)/dy,tb=(y+radius-ay)/dy;
            lo=fmaxf(0,fminf(ta,tb));hi=fminf(1,fmaxf(ta,tb));if(lo>hi)continue;
        }else if(fabsf(y-ay)>radius)continue;
        float xa=ax+dx*lo,xb=ax+dx*hi;
        int x0=(int)floorf(fminf(xa,xb)-radius),x1=(int)ceilf(fmaxf(xa,xb)+radius);
        if(x0<0)x0=0;
        if(x1>=w)x1=w-1;
        for(int x=x0;x<=x1;x++){
            float t=len2>.0001f?fmaxf(0,fminf(1,((x-ax)*dx+(y-ay)*dy)/len2)):(za>zb?0:1);
            float nx=x-(ax+t*dx),ny=y-(ay+t*dy),dist2=nx*nx+ny*ny;
            if(dist2>r2)continue;
            float z=za+(zb-za)*t;
            size_t index=(size_t)y*w+x;
            if(z+(shaded?depth_radius:0)<=depth[index])continue;
            float curve=shaded?sqrtf(fmaxf(0,1-dist2/r2)):0;
            z+=shaded?curve*depth_radius:0;
            if(z<=depth[index])continue;
            depth[index]=z;
            float light=shaded?fmaxf(.28f,fminf(1,.48f+.48f*curve-.18f*ny/radius)):1;
            unsigned red=(unsigned)((color>>11)*light),green=(unsigned)(((color>>5)&63)*light),blue=(unsigned)((color&31)*light);
            pixels[index]=(uint16_t)((red<<11)|(green<<5)|blue);
        }
    }
}
static size_t render(const gcode_toolpath_t *m,const gcode_view_t *v,uint16_t *pixels,float *depth,int w,int h,size_t first,size_t budget)
{
    if(!pixels||w<1||h<1||w>2048||h>2048||!budget)return 0;
    if(!first){
        for(size_t i=0;i<(size_t)w*h;i++)pixels[i]=0x0863;
        if(depth)for(size_t i=0;i<(size_t)w*h;i++)depth[i]=-FLT_MAX;
    }
    if(!m||!v||!m->count||!isfinite(v->yaw)||!isfinite(v->pitch)||!isfinite(v->zoom)||!isfinite(v->pan_x)||!isfinite(v->pan_y))return 0;
    float cx=(m->min.x+m->max.x)*.5f,cy=(m->min.y+m->max.y)*.5f,cz=(m->min.z+m->max.z)*.5f;
    float radius=sqrtf(powf(m->max.x-m->min.x,2)+powf(m->max.y-m->min.y,2)+powf(m->max.z-m->min.z,2))*.5f;
    float zoom=fmaxf(.25f,fminf(v->zoom,GCODE_VIEW_ZOOM_MAX));float scale=.43f*(float)(w<h?w:h)/fmaxf(radius,.1f)*zoom;
    float sy=sinf(v->yaw),co=cosf(v->yaw),sp=sinf(v->pitch),cp=cosf(v->pitch);
    size_t extras=m->sample_stride>1?m->layer_count+GCODE_TOOLPATH_TOOLS_MAX:0;
    size_t total=m->count+extras+(extras?1:0);
    if(first>=total)return 0;
    size_t end=total-first>budget?first+budget:total;
    for(size_t i=first;i<end;i++){
        const gcode_segment_t *s;
        if(i<extras){
            if(i<m->layer_count)s=&m->layer_anchor[i];
            else{unsigned tool=(unsigned)(i-m->layer_count);if(!(m->tool_anchor_mask&(1u<<tool)))continue;s=&m->tool_anchor[tool];}
        }else if(i<extras+m->count)s=&m->segments[depth?m->count-1-(i-extras):i-extras];else s=&m->last_segment;
        if((!v->travel&&!s->extrusion)||(v->layer>=0&&(v->cumulative?s->layer>v->layer:s->layer!=v->layer)))continue;
        int xy[4];float zdepth[2];gcode_point_t points[2]={s->a,s->b};
        for(int j=0;j<2;j++){
            float x=points[j].x-cx,y=points[j].y-cy,z=points[j].z-cz;
            zdepth[j]=(x*sy+y*co)*cp+z*sp;
            xy[j*2]=(int)lroundf(w*(.5f+fmaxf(-32,fminf(32,v->pan_x)))+(x*co-y*sy)*scale);
            xy[j*2+1]=(int)lroundf(h*(.5f+fmaxf(-32,fminf(32,v->pan_y)))+((x*sy+y*co)*sp-z*cp)*scale);
        }
        /* Zoom is bounded; projected endpoints stay within a few canvas spans. */
        static const uint16_t tools[]={0x07ff,0xfd20,0x07e0,0xf81f,0x4bff,0xffe0,0xf800,0xffff};
        uint16_t color=s->extrusion?(s->tool?tools[s->tool%8]:(uint16_t)(0x07ff-((s->layer%12)<<5))):0x52aa;
        if(depth&&s->extrusion)color=tools[s->tool%8];
        if(depth){float radius=s->extrusion?fminf(8,fmaxf(.65f,(s->width>0?s->width*.5f:.225f)*scale)):.51f;gcode_toolpath_raster_bead(pixels,depth,w,h,xy[0],xy[1],xy[2],xy[3],zdepth[0],zdepth[1],radius,radius/scale,color,s->extrusion);}
        else line(pixels,w,h,xy[0],xy[1],xy[2],xy[3],color);
    }
    return end<total?end:0;
}

void gcode_toolpath_render(const gcode_toolpath_t *m,const gcode_view_t *v,uint16_t *pixels,int w,int h)
{(void)render(m,v,pixels,NULL,w,h,0,SIZE_MAX);}
void gcode_toolpath_render_solid(const gcode_toolpath_t *m,const gcode_view_t *v,uint16_t *pixels,float *depth,int w,int h)
{(void)render(m,v,pixels,depth,w,h,0,SIZE_MAX);}

size_t gcode_toolpath_render_solid_step(const gcode_toolpath_t *m,const gcode_view_t *v,uint16_t *pixels,float *depth,int w,int h,size_t first,size_t budget)
{return render(m,v,pixels,depth,w,h,first,budget);}

void gcode_toolpath_collect(gcode_toolpath_t *m,gcode_segment_t s){if(!m->capacity||!m->segments)return;m->input_segments++;m->last_segment=s;retain(m,s);}
