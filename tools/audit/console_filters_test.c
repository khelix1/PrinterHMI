#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "console_filter.h"
static console_entry_t entry;
static bool matches(console_entry_type_t type, const char *text, console_filter_kind_t kind, const char *query, bool hide)
{
    entry.type=type;
    snprintf(entry.message,sizeof(entry.message),"%s",text);
    return console_filter_matches(&entry,kind,query,hide);
}
int main(void)
{
    for (int type=CONSOLE_ENTRY_COMMAND;type<=CONSOLE_ENTRY_SYSTEM;++type) {
        assert(matches(type,"probe TEST",CONSOLE_FILTER_ALL,"test",false));
        assert(!matches(type,"probe TEST",CONSOLE_FILTER_ALL,"xyz",false));
        assert(matches(type,"probe TEST",CONSOLE_FILTER_ALERTS,NULL,false)==(type==CONSOLE_ENTRY_ERROR || type==CONSOLE_ENTRY_WARNING));
        const console_filter_kind_t kinds[]={CONSOLE_FILTER_COMMANDS,CONSOLE_FILTER_RESPONSES,CONSOLE_FILTER_WARNINGS,CONSOLE_FILTER_ERRORS,CONSOLE_FILTER_SYSTEM};
        for (int i=0;i<5;++i) assert(matches(type,"probe TEST",kinds[i],"",false)==(type==i));
    }
    const char *reports[]={"T:200.0 /205.0 B:60.0 /60.0", "ok T0:20 /0 T1:30 /0 @:0 B@:0", "// T: -5.2 / 0 B:20 /0", "B:20", "T:200 /205"};
    for(size_t i=0;i<sizeof(reports)/sizeof(*reports);++i){
        assert(!matches(CONSOLE_ENTRY_RESPONSE,reports[i],CONSOLE_FILTER_ALL,"",true));
        assert(matches(CONSOLE_ENTRY_RESPONSE,reports[i],CONSOLE_FILTER_ALL,"",false));
        assert(matches(CONSOLE_ENTRY_WARNING,reports[i],CONSOLE_FILTER_ALL,"",true));
        assert(matches(CONSOLE_ENTRY_ERROR,reports[i],CONSOLE_FILTER_ALL,"",true));
    }
    const char *other[]={"T:200 heater unstable","T:nan","T:inf","T:20 B:invalid","T:20garbage","T:","T", "ok", "@:0", "Temperature sensor read error", "probe accuracy results: average 1.2", "TMC stepper_x status"};
    for(size_t i=0;i<sizeof(other)/sizeof(*other);++i) assert(matches(CONSOLE_ENTRY_RESPONSE,other[i],CONSOLE_FILTER_ALL,NULL,true));
    entry.type=CONSOLE_ENTRY_RESPONSE;strcpy(entry.message,"T:20");
    console_entry_t before=entry;
    assert(!console_filter_matches(&entry,CONSOLE_FILTER_ALL,"",true));
    assert(!memcmp(&entry,&before,sizeof(entry)));
    assert(!console_filter_matches(NULL,CONSOLE_FILTER_ALL,NULL,false));
    assert(!console_filter_matches(&entry,CONSOLE_FILTER_COUNT,NULL,false));
    puts("PASS: Console type/text/temperature filters and history preservation");
}
