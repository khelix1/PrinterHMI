static void host_discovery_complete(lv_timer_t *timer)
{
 if(timer!=s_scan_task_handle){lv_timer_delete(timer);return;}
 s_scan_task_handle=NULL;lv_timer_delete(timer);
 if(moonraker_discovery_is_cancelled()||!s_popup||!s_results_list)return;
 int mode=host_discovery_mode();
 if(mode==1){moonraker_discovery_set_status("No Moonraker endpoints found (sample). Tap Retry.");return;}
 moonraker_probe_result_t first={.reachable=true,.klippy_ready=true};
 copy_text(first.identity,sizeof(first.identity),"Sermoon D1");
 moonraker_discovery_add_candidate("127.0.0.1",7125,&first);
 copy_text(first.identity,sizeof(first.identity),"Sermoon D1 #2");
 moonraker_discovery_add_candidate("127.0.0.1",7126,&first);
 moonraker_discovery_set_status("2 local sample endpoints found. Select a printer.");
}
bool moonraker_discovery_start(const esp_ip4_addr_t *ip)
{
 if(!s_popup||!s_results_list||s_discovery_dispatching||s_owner_generation!=moonraker_config_generation())return false;
 s_has_scan_ip=ip!=NULL;if(ip)s_retry_ip=*ip;
 if(!ip||s_scan_task_handle){moonraker_discovery_set_status("Connect Wi-Fi or wait for the previous sample scan to finish.");return false;}
 s_scan_ip=*ip;s_cancelled=false;s_scan_epoch=s_discovery_epoch;
 if(host_discovery_mode()==2){moonraker_discovery_set_status("Could not start sample discovery. Tap Retry.");return false;}
 moonraker_discovery_set_status("Searching local sample endpoints...");
 s_scan_task_handle=lv_timer_create(host_discovery_complete,300,NULL);
 return s_scan_task_handle!=NULL;
}
