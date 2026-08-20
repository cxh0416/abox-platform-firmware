#include "abox_boot_v2_app.h"
#include "abox_platform_port.h"
#include <stdio.h>
#include <string.h>

typedef enum {
 ST_WAIT,ST_PROBE,ST_CA_DELETE,ST_CA_UPLOAD,ST_IDLE,ST_MQTT_DISC,ST_MQTT_CLOSE,ST_HTTP_STOP,ST_PDP_DEACT,ST_PDP_ACT,
 ST_TLS_VERSION,ST_TLS_LEVEL,ST_TLS_SNI,ST_TLS_CA,ST_HTTP_CONTEXT,ST_HTTP_TLS,ST_URL,ST_FILE_DELETE,ST_FILE_OPEN,
 ST_RANGE_GET,ST_HTTP_READ,ST_FILE_WRITE,ST_FILE_CLOSE,ST_FILE_LIST,ST_VERIFY_OPEN,ST_VERIFY_READ,ST_VERIFY_CLOSE,
 ST_VERIFY,ST_RETRY,ST_ERROR
} State;
typedef struct {
 ABoxBootV2AppPort port; ABoxBootV2AppConfig cfg; State state; uint8_t bound,provisioned,busy,seeding,slot,handle_valid;
 uint8_t install_ready,failure_pending,payload_sent,vector[8],vector_len;
 uint8_t probe_index,seed_attempted,file_seen; uint32_t handle,expected_size,expected_crc,downloaded,chunk_size,chunk_received;
 uint32_t read_size,crc,http_status,http_length,written,written_total,file_size,last_error,retry_due;
 char url[ABOX_BOOT_V2_APP_URL_SIZE],version[ABOX_BOOT_V2_APP_VERSION_SIZE];
} Context;
/* Keep the 1 KiB transfer buffer outside the normal BSS.  Product linker
 * scripts place this reset-scratch workspace after the fixed fault mailbox. */
static Context g __attribute__((section(".ota_work")));
static const char*const probes[]={"AT+QFLDS=\"UFS\"","AT+QHTTPGETEX=?","AT+QHTTPREAD=?","AT+QFOPEN=?","AT+QFREAD=?","AT+QFWRITE=?","AT+QFCLOSE=?","AT+QFDEL=?","AT+QFUPL=?","AT+QSSLCFG=?","AT+QHTTPCFG=?"};
static const char*slot_name(uint8_t s){return s?"fw_slot1.bin":"fw_slot0.bin";}
static uint32_t now(void){return g.port.tick_ms?g.port.tick_ms(g.port.context):0U;}
static void logmsg(uint8_t level,const char*msg){if(g.port.log)g.port.log(g.port.context,level,msg);}
static uint32_t crc_update(uint32_t c,const uint8_t*p,uint32_t n){while(n--){uint8_t b;c^=*p++;for(b=0;b<8U;b++)c=(c>>1U)^(0xEDB88320U&(0U-(c&1U)));}return c;}
static int valid_cfg(void){return g.cfg.ota_origin&&strncmp(g.cfg.ota_origin,"https://",8U)==0&&g.cfg.revision_product&&g.cfg.artifact_name&&g.cfg.running_version&&g.cfg.ca_pem&&g.cfg.ca_pem_length&&g.cfg.app_size&&g.cfg.app_size<=g.cfg.app_max_size;}
static void done(ABoxBootV2AtResult result,void*user);
static int submit(const char*cmd,uint32_t timeout){g.payload_sent=0U;if(!g.port.submit||!g.port.submit(g.port.context,cmd,timeout,done,0)){g.last_error=ABOX_BOOT_V2_APP_ERROR_UFS;g.state=ST_ERROR;return 0;}return 1;}
static void resume_mqtt(void){if(g.port.mqtt_pause)g.port.mqtt_pause(g.port.context,0U);}
static void fail(uint32_t error)
{
 if (g.port.cancel) g.port.cancel(g.port.context);
 resume_mqtt();g.last_error=error;if(g.busy)g.failure_pending=1U;g.busy=0U;g.handle_valid=0U;g.retry_due=now()+300000U;g.state=ST_ERROR;logmsg(3U,"Boot V2 App staging failed");
}
static void begin_download(void)
{
 if (g.port.mqtt_pause) g.port.mqtt_pause(g.port.context,1U);
 g.state=ST_MQTT_DISC;(void)submit("AT+QMTDISC=0",10000U);
}
static int begin_request(const ABoxBootV2AppRequest*r,uint8_t seeding)
{
 ABoxBootV2Record s; char expected[ABOX_BOOT_V2_APP_URL_SIZE]; int n;
 if(!r||!g.provisioned||g.busy||r->size==0U||r->size>g.cfg.app_max_size||strlen(r->version)==0U||strlen(r->version)>=ABOX_BOOT_V2_APP_VERSION_SIZE||strncmp(r->version,g.cfg.version_prefix,strlen(g.cfg.version_prefix))!=0){g.last_error=ABOX_BOOT_V2_APP_ERROR_URL;return 0;}
 n=snprintf(expected,sizeof(expected),"%s/fw-revisions/%s/%s/%s",g.cfg.ota_origin,g.cfg.revision_product,r->version,g.cfg.artifact_name);if(n<=0||(uint32_t)n>=sizeof(expected)||strcmp(expected,r->url)!=0){g.last_error=ABOX_BOOT_V2_APP_ERROR_URL;return 0;}
 memset(&s,0,sizeof(s));if(ABoxBootV2_StateLoad(&s)&&!seeding&&(s.state!=ABOX_BOOT_V2_NORMAL&&s.state!=ABOX_BOOT_V2_CONFIRMED)){g.last_error=ABOX_BOOT_V2_APP_ERROR_STATE;return 0;}
 g.seeding=seeding;g.slot=seeding?0U:(s.stable_slot==0U?1U:0U);g.expected_size=r->size;g.expected_crc=r->crc32;g.downloaded=0U;g.read_size=0U;g.crc=0xFFFFFFFFU;g.vector_len=0U;g.file_seen=0U;g.file_size=0U;g.handle_valid=0U;g.failure_pending=0U;g.busy=1U;g.last_error=0U;
 (void)snprintf(g.url,sizeof(g.url),"%s",r->url);(void)snprintf(g.version,sizeof(g.version),"%s",r->version);begin_download();return 1;
}
static void next_after_ok(void)
{
 char cmd[128];
 switch(g.state){
 case ST_PROBE: if(++g.probe_index<(uint8_t)(sizeof(probes)/sizeof(probes[0])))submit(probes[g.probe_index],5000U);else{g.state=ST_CA_DELETE;submit("AT+QFDEL=\"ota_ca.pem\"",5000U);}break;
 case ST_CA_DELETE:g.state=ST_CA_UPLOAD;(void)snprintf(cmd,sizeof(cmd),"AT+QFUPL=\"ota_ca.pem\",%lu,30",(unsigned long)g.cfg.ca_pem_length);submit(cmd,40000U);break;
 case ST_CA_UPLOAD:g.provisioned=1U;g.state=ST_IDLE;g.last_error=0U;logmsg(1U,"Boot V2 App transport ready");break;
 case ST_MQTT_DISC:g.state=ST_MQTT_CLOSE;submit("AT+QMTCLOSE=0",10000U);break;
 case ST_MQTT_CLOSE:g.state=ST_HTTP_STOP;submit("AT+QHTTPSTOP",10000U);break;
 case ST_HTTP_STOP:g.state=ST_PDP_DEACT;submit("AT+QIDEACT=1",40000U);break;
 case ST_PDP_DEACT:g.state=ST_PDP_ACT;submit("AT+QIACT=1",150000U);break;
 case ST_PDP_ACT:g.state=ST_TLS_VERSION;submit("AT+QSSLCFG=\"sslversion\",1,3",5000U);break;
 case ST_TLS_VERSION:g.state=ST_TLS_LEVEL;break;case ST_TLS_LEVEL:g.state=ST_TLS_SNI;break;case ST_TLS_SNI:g.state=ST_TLS_CA;break;case ST_TLS_CA:g.state=ST_HTTP_CONTEXT;break;case ST_HTTP_CONTEXT:g.state=ST_HTTP_TLS;break;case ST_HTTP_TLS:g.state=ST_URL;break;
 case ST_URL:g.state=ST_FILE_DELETE;(void)snprintf(cmd,sizeof(cmd),"AT+QFDEL=\"%s\"",slot_name(g.slot));submit(cmd,5000U);break;
 case ST_FILE_DELETE:g.state=ST_FILE_OPEN;(void)snprintf(cmd,sizeof(cmd),"AT+QFOPEN=\"%s\",1",slot_name(g.slot));submit(cmd,5000U);break;
 case ST_FILE_OPEN:if(!g.handle_valid)fail(ABOX_BOOT_V2_APP_ERROR_UFS);else g.state=ST_RANGE_GET;break;
 case ST_RANGE_GET:if(g.http_status!=206U||g.http_length!=g.chunk_size)fail(ABOX_BOOT_V2_APP_ERROR_HTTP);else{g.state=ST_HTTP_READ;submit("AT+QHTTPREAD=80",90000U);}break;
 case ST_HTTP_READ:if(g.chunk_received!=g.chunk_size)fail(ABOX_BOOT_V2_APP_ERROR_SIZE);else{g.state=ST_FILE_WRITE;(void)snprintf(cmd,sizeof(cmd),"AT+QFWRITE=%lu,%lu,10",(unsigned long)g.handle,(unsigned long)g.chunk_size);submit(cmd,15000U);}break;
 case ST_FILE_WRITE:if(g.written!=g.chunk_size||g.written_total!=g.downloaded+g.chunk_size)fail(ABOX_BOOT_V2_APP_ERROR_UFS);else{g.downloaded+=g.chunk_size;if(g.downloaded==g.expected_size){g.state=ST_FILE_CLOSE;(void)snprintf(cmd,sizeof(cmd),"AT+QFCLOSE=%lu",(unsigned long)g.handle);submit(cmd,5000U);}else g.state=ST_RANGE_GET;}break;
 case ST_FILE_CLOSE:g.handle_valid=0U;g.state=ST_FILE_LIST;(void)snprintf(cmd,sizeof(cmd),"AT+QFLST=\"%s\"",slot_name(g.slot));submit(cmd,5000U);break;
 case ST_FILE_LIST:if(!g.file_seen||g.file_size!=g.expected_size)fail(ABOX_BOOT_V2_APP_ERROR_SIZE);else{g.state=ST_VERIFY_OPEN;(void)snprintf(cmd,sizeof(cmd),"AT+QFOPEN=\"%s\",0",slot_name(g.slot));submit(cmd,5000U);}break;
 case ST_VERIFY_OPEN:if(!g.handle_valid)fail(ABOX_BOOT_V2_APP_ERROR_UFS);else g.state=ST_VERIFY_READ;break;
 case ST_VERIFY_READ:if(g.read_size==g.expected_size){g.state=ST_VERIFY_CLOSE;(void)snprintf(cmd,sizeof(cmd),"AT+QFCLOSE=%lu",(unsigned long)g.handle);submit(cmd,5000U);}break;
 case ST_VERIFY_CLOSE:g.handle_valid=0U;g.state=ST_VERIFY;break;default:break;}
}
static void done(ABoxBootV2AtResult result,void*user)
{
 (void)user;if(result!=ABOX_BOOT_V2_AT_OK){if((g.state==ST_CA_DELETE||g.state==ST_FILE_DELETE||g.state==ST_HTTP_STOP)&&result==ABOX_BOOT_V2_AT_ERROR){next_after_ok();return;}fail(g.state<=ST_CA_UPLOAD?ABOX_BOOT_V2_APP_ERROR_CA:ABOX_BOOT_V2_APP_ERROR_UFS);return;}next_after_ok();
}
static void event(ABoxBootV2AtEvent e,const uint8_t*data,uint16_t len,void*user)
{
 char line[96];(void)user;if(e==ABOX_BOOT_V2_AT_RAW){if(g.state==ST_HTTP_READ){if(g.chunk_received+len>g.chunk_size){fail(ABOX_BOOT_V2_APP_ERROR_SIZE);return;}memcpy(g.port.transfer_buffer+g.chunk_received,data,len);g.chunk_received+=len;}else if(g.state==ST_VERIFY_READ){uint16_t c=len;if(g.vector_len<8U){if(c>8U-g.vector_len)c=(uint16_t)(8U-g.vector_len);memcpy(g.vector+g.vector_len,data,c);g.vector_len=(uint8_t)(g.vector_len+c);}g.crc=crc_update(g.crc,data,len);g.read_size+=len;}return;}
 if (len>=sizeof(line)) len=sizeof(line)-1U;
 memcpy(line,data,len);line[len]=0;
 if(strncmp(line,"+QFOPEN:",8U)==0){unsigned long h;if(sscanf(line,"+QFOPEN: %lu",&h)==1){g.handle=(uint32_t)h;g.handle_valid=1U;}}
 else if(strncmp(line,"+QHTTPGET:",10U)==0){unsigned long r,s,l;if(sscanf(line,"+QHTTPGET: %lu,%lu,%lu",&r,&s,&l)==3&&r==0U){g.http_status=(uint32_t)s;g.http_length=(uint32_t)l;}}
 else if(strncmp(line,"+QFWRITE:",9U)==0){unsigned long w,t;if(sscanf(line,"+QFWRITE: %lu,%lu",&w,&t)==2){g.written=(uint32_t)w;g.written_total=(uint32_t)t;}}
 else if(strncmp(line,"+QFLST:",7U)==0){char name[32];unsigned long s;if(sscanf(line,"+QFLST: \"%31[^\"]\",%lu",name,&s)==2&&strstr(name,slot_name(g.slot))){g.file_seen=1U;g.file_size=(uint32_t)s;}}
 if(strcmp(line,"CONNECT")==0&&g.state==ST_HTTP_READ&&g.port.begin_raw_read)g.port.begin_raw_read(g.port.context,g.chunk_size);
 else if(strncmp(line,"CONNECT ",8U)==0&&g.state==ST_VERIFY_READ){unsigned long n;if(sscanf(line,"CONNECT %lu",&n)==1&&n>0U&&n<=ABOX_BOOT_V2_APP_RAW_CHUNK&&g.port.begin_raw_read)g.port.begin_raw_read(g.port.context,(uint32_t)n);}
}
int ABoxBootV2App_Init(const ABoxBootV2AppPort*p,const ABoxBootV2AppConfig*c){if(!p||!c||!p->submit||!p->register_events||!p->transfer_buffer||p->transfer_buffer_size<ABOX_BOOT_V2_APP_RAW_CHUNK)return 0;memset(&g,0,sizeof(g));g.port=*p;g.cfg=*c;g.bound=1U;g.state=ST_WAIT;p->register_events(p->context,event,0);return valid_cfg();}
int ABoxBootV2App_BeginProvisioning(void){if(!g.bound)return 0;g.probe_index=0U;g.provisioned=0U;g.state=ST_PROBE;return submit(probes[0],5000U);}
int ABoxBootV2App_Start(const ABoxBootV2AppRequest*r){return begin_request(r,0U);}
static int need_seed(void){ABoxBootV2Record s;if(!ABoxBootV2_StateLoad(&s))return 1;if(s.state!=ABOX_BOOT_V2_CONFIRMED&&s.state!=ABOX_BOOT_V2_NORMAL)return 0;return s.stable_image_size!=g.cfg.app_size||s.stable_image_crc32!=ABoxBootV2_Crc32((const void*)(uintptr_t)g.cfg.app_start_addr,g.cfg.app_size)||strncmp(s.stable_image_version,g.cfg.running_version,sizeof(s.stable_image_version))!=0;}
void ABoxBootV2App_Task(void)
{
 char cmd[128];ABoxBootV2Record s;
 if((g.state==ST_ERROR||g.state==ST_RETRY)&&(int32_t)(now()-g.retry_due)>=0){(void)ABoxBootV2App_BeginProvisioning();return;}
 if(g.provisioned&&!g.busy&&!g.seed_attempted&&need_seed()){ABoxBootV2AppRequest r;memset(&r,0,sizeof(r));g.seed_attempted=1U;(void)snprintf(r.url,sizeof(r.url),"%s/fw-revisions/%s/%s/%s",g.cfg.ota_origin,g.cfg.revision_product,g.cfg.running_version,g.cfg.artifact_name);(void)snprintf(r.version,sizeof(r.version),"%s",g.cfg.running_version);r.size=g.cfg.app_size;r.crc32=ABoxBootV2_Crc32((const void*)(uintptr_t)g.cfg.app_start_addr,g.cfg.app_size);if(!begin_request(&r,1U))fail(ABOX_BOOT_V2_APP_ERROR_STATE);return;}
 if(g.port.is_active&&g.port.is_active(g.port.context)&&!g.payload_sent){if(g.state==ST_CA_UPLOAD){g.payload_sent=(uint8_t)g.port.send_payload(g.port.context,g.cfg.ca_pem,(uint16_t)g.cfg.ca_pem_length);return;}if(g.state==ST_URL){g.payload_sent=(uint8_t)g.port.send_payload(g.port.context,(const uint8_t*)g.url,(uint16_t)strlen(g.url));return;}if(g.state==ST_FILE_WRITE){g.payload_sent=(uint8_t)g.port.send_payload(g.port.context,g.port.transfer_buffer,(uint16_t)g.chunk_size);return;}}
 if(g.port.has_pending&&g.port.has_pending(g.port.context))return;
 switch(g.state){case ST_TLS_LEVEL:submit("AT+QSSLCFG=\"seclevel\",1,1",5000U);break;case ST_TLS_SNI:submit("AT+QSSLCFG=\"sni\",1,1",5000U);break;case ST_TLS_CA:submit("AT+QSSLCFG=\"cacert\",1,\"UFS:ota_ca.pem\"",5000U);break;case ST_HTTP_CONTEXT:submit("AT+QHTTPCFG=\"contextid\",1",5000U);break;case ST_HTTP_TLS:submit("AT+QHTTPCFG=\"sslctxid\",1",5000U);break;case ST_URL:(void)snprintf(cmd,sizeof(cmd),"AT+QHTTPURL=%u,80",(unsigned)strlen(g.url));submit(cmd,10000U);break;case ST_RANGE_GET:g.chunk_size=g.expected_size-g.downloaded;if(g.chunk_size>ABOX_BOOT_V2_APP_RAW_CHUNK)g.chunk_size=ABOX_BOOT_V2_APP_RAW_CHUNK;g.http_status=0U;g.http_length=0U;(void)snprintf(cmd,sizeof(cmd),"AT+QHTTPGETEX=80,%lu,%lu",(unsigned long)g.downloaded,(unsigned long)g.chunk_size);submit(cmd,90000U);break;case ST_HTTP_READ:g.chunk_received=0U;break;case ST_VERIFY_READ:{uint32_t n=g.expected_size-g.read_size;if(n>ABOX_BOOT_V2_APP_RAW_CHUNK)n=ABOX_BOOT_V2_APP_RAW_CHUNK;(void)snprintf(cmd,sizeof(cmd),"AT+QFREAD=%lu,%lu",(unsigned long)g.handle,(unsigned long)n);submit(cmd,10000U);break;}case ST_VERIFY:g.crc^=0xFFFFFFFFU;if(g.read_size!=g.expected_size)fail(ABOX_BOOT_V2_APP_ERROR_SIZE);else if(g.crc!=g.expected_crc)fail(ABOX_BOOT_V2_APP_ERROR_CRC);else if(!ABoxBootV2_ImageVectorValid(g.vector))fail(ABOX_BOOT_V2_APP_ERROR_VECTOR);else{memset(&s,0,sizeof(s));(void)ABoxBootV2_StateLoad(&s);s.candidate_slot=g.slot;s.image_size=g.expected_size;s.image_crc32=g.expected_crc;(void)snprintf(s.image_version,sizeof(s.image_version),"%s",g.version);if(g.seeding){s.state=ABOX_BOOT_V2_CONFIRMED;s.stable_slot=0U;s.stable_image_size=g.expected_size;s.stable_image_crc32=g.expected_crc;(void)snprintf(s.stable_image_version,sizeof(s.stable_image_version),"%s",g.version);}else s.state=ABOX_BOOT_V2_INSTALL_PENDING;if(!ABoxBootV2_StateSave(&s))fail(ABOX_BOOT_V2_APP_ERROR_STATE);else{g.busy=0U;g.install_ready=(uint8_t)!g.seeding;g.state=ST_IDLE;resume_mqtt();}}break;default:break;}
}
int ABoxBootV2App_IsProvisioned(void){return g.provisioned;}
int ABoxBootV2App_IsReady(void){ABoxBootV2Record s;ABoxBootV2Descriptor d;return g.provisioned&&ABoxBootV2_DescriptorRead(&d)&&ABoxBootV2_DescriptorReady(&d)&&ABoxBootV2_StateLoad(&s)&&s.stable_slot<=1U&&s.stable_image_size!=0U;}
int ABoxBootV2App_IsBusy(void){return g.busy;}uint32_t ABoxBootV2App_LastError(void){return g.last_error;}const char*ABoxBootV2App_TargetVersion(void){return g.version;}
const char*ABoxBootV2App_Phase(void){if(g.state==ST_WAIT)return"WAIT_MODEM";if(g.state<=ST_CA_UPLOAD)return"PROVISIONING";if(g.state>=ST_MQTT_DISC&&g.state<=ST_FILE_CLOSE)return"DOWNLOADING";if(g.state>=ST_FILE_LIST&&g.state<=ST_VERIFY)return"VERIFYING";if(g.state==ST_ERROR)return"ERROR";return"IDLE";}
int ABoxBootV2App_TakeInstallReady(void){int v=g.install_ready;g.install_ready=0U;return v;}int ABoxBootV2App_TakeFailure(uint32_t*e){int v=g.failure_pending;g.failure_pending=0U;if(e)*e=g.last_error;return v;}
