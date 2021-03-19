/*
 *  ____  ____      _    ____ ___ _   _  ___  
 *  |  _ \|  _ \    / \  / ___|_ _| \ | |/ _ \ 
 *  | | | | |_) |  / _ \| |  _ | ||  \| | | | |
 *  | |_| |  _ <  / ___ \ |_| || || |\  | |_| |
 *  |____/|_| \_\/_/   \_\____|___|_| \_|\___/ 
 *
 * location service -- An opensource of lorawan location service 
 *
 * See http://www.dragino.com for more information about
 * the lora gateway project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 *
 * Maintainer: skerlan
 *
 */

/*! \file
 *
 * \brief lora location service
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <signal.h> 
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h> 
#include <string.h>
#include <curl/curl.h>

#include "MQTTAsync.h"
#include "parson.h"
#include "linkedlists.h"
#include "utilities.h"
#include "location.h"
#include "mapwize_api.h"

#define DEFAULT_MQTT_CLIENTID     "DRAGINO_MQTT_CLIENT"
#define DEFAULT_URL_LEN           100
#define DEFAULT_LOOP_MS           10000UL   
#define DEFUALT_KEEPALIVE         5000L
#define TIMEOUT                   10000L

/* -------------------------------------------------------------------------- */
/* --- VARIABLES (GLOBAL) ------------------------------------------- */
uint8_t LOG_INFO = 1;
uint8_t LOG_WARNING = 1;
uint8_t LOG_ERROR = 0;
uint8_t LOG_DEBUG = 0;
uint8_t LOG_MEM = 0;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE VARIABLES (GLOBAL) ------------------------------------------- */

/* signal handling variables */
volatile bool exit_sig = false; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
volatile bool quit_sig = false; /* 1 -> application terminates without shutting down the hardware */

/* location configure */
loccfg_s loccfg = LOCCFG_INIT;

int disc_finished = 0;
int subscribed = 0;
int finished = 0;

/* define a list head for payload */
LGW_LIST_HEAD_STATIC(payload_list, _payload_s);

/* define a list head for payload */
LGW_LIST_HEAD_STATIC(inode_list, _inode_s);

/* define a list head for ibeacon */
LGW_LIST_HEAD_NOLOCK_STATIC(ibeacon_list, _ibeacon_s);

/* define payload parse sem */
sem_t parse_payload_sem;

/* define payload parse sem */
sem_t parse_inode_sem;

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DECLARATION ---------------------------------------- */
static int parse_serv_cfg(const char * conf_file);
static void cfg_clean(loccfg_s* cfg);

static int get_beacons(curlstr_s* cstr);
static int get_placetype(curlstr_s* cstr);
// mqtt connect function
static void connlost(void *context, char *cause);
static int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message);
static void onDisconnectFailure(void* context, MQTTAsync_failureData* response);
static void onDisconnect(void* context, MQTTAsync_successData* response);
static void onSubscribe(void* context, MQTTAsync_successData* response);
static void onSubscribeFailure(void* context, MQTTAsync_failureData* response);
static void onConnectFailure(void* context, MQTTAsync_failureData* response);
static void onConnect(void* context, MQTTAsync_successData* response);

static void thread_parse_payload(void);
static void thread_create_place();


static float calc_dist_byrssi(int rssi, int rate, float div);
static void free_inode_entry(inode_s* node);
static void free_cfg_entry(loccfg_s* cfg);

/* -------------------------------------------------------------------------- */
/* --- PRIVATE FUNCTIONS DEFINITION ----------------------------------------- */
static void sig_handler(int sigio) {
    if (sigio == SIGQUIT) {
        quit_sig = true;
    } else if ((sigio == SIGINT) || (sigio == SIGTERM)) {
        exit_sig = true;
    }
    return;
}

static int parse_serv_cfg(const char * conf_file) {
    JSON_Value *root_val;
    JSON_Object *conf_obj = NULL;
    JSON_Object *serv_obj = NULL;
    JSON_Array *serv_arry = NULL;
    JSON_Value *val = NULL; /* needed to detect the absence of some fields */
    const char *str; /* pointer to sub-strings in the JSON data */
	
    /* try to parse JSON */
    root_val = json_parse_file_with_comments(conf_file);
    if (root_val == NULL) {
        MSG_DEBUG(LOG_ERROR, "ERROR~ %s is not a valid JSON file\n", conf_file);
        exit(EXIT_FAILURE);
    }

    /* point to the gateway configuration object */
    conf_obj = json_object_get_object(json_value_get_object(root_val), "mqtt_conf");
    if (conf_obj == NULL) {
        MSG_DEBUG(LOG_INFO, "INFO~ %s does not contain a JSON object named mqtt_confs\n", conf_file);
        return -1;
    } else {
        MSG_DEBUG(LOG_INFO, "INFO~ %s does contain a JSON object named mqtt_confs, parsing mqtt parameters\n", conf_file);
    }

    /* server address identifier */
    //TODO many servers suport
    str = json_object_get_string(conf_obj, "servaddr");
    if (str != NULL) {
        lgw_free(loccfg.servaddr);
        loccfg.servaddr = lgw_strdup(str);
        MSG_DEBUG(LOG_INFO, "INFO~ mqtt server address is configured to %s\n", loccfg.servaddr);
    }

    val = json_object_get_value(conf_obj, "servport");
    if (val != NULL) {
        loccfg.servport = (uint16_t)json_value_get_number(val);
        MSG_DEBUG(LOG_INFO, "INFO~ mqtt server port is configured to %u\n", loccfg.servport);
    } 

    val = json_object_get_value(conf_obj, "qos");
    if (val != NULL) {
        loccfg.qos = (int)json_value_get_number(val);
        MSG_DEBUG(LOG_INFO, "INFO~ mqtt server QOS is configured to %d\n", loccfg.qos);
    } 

    str = json_object_get_string(conf_obj, "clientid");
    if (str != NULL) {
        loccfg.clientid = lgw_strdup(str);
    } else 
        loccfg.clientid = lgw_strdup(DEFAULT_MQTT_CLIENTID);

    MSG_DEBUG(LOG_INFO, "INFO~ mqtt server address is configured to %s\n", loccfg.clientid);

    str = json_object_get_string(conf_obj, "username");
    if (str != NULL) {
        loccfg.username = lgw_strdup(str);
        MSG_DEBUG(LOG_INFO, "INFO~ mqtt username is configured to %s\n", loccfg.username);
    }

    str = json_object_get_string(conf_obj, "password");
    if (str != NULL) {
        loccfg.password = lgw_strdup(str);
        MSG_DEBUG(LOG_INFO, "INFO~ mqtt password is configured to %s\n", loccfg.password);
    }

    str = json_object_get_string(conf_obj, "topic");
    if (str != NULL) {
        loccfg.topic = lgw_strdup(str);
        MSG_DEBUG(LOG_INFO, "INFO~ mqtt password is configured to %s\n", loccfg.topic);
    }

    conf_obj = json_object_get_object(json_value_get_object(root_val), "mapwize_conf");
    if (conf_obj == NULL) {
        MSG_DEBUG(LOG_INFO, "INFO~ %s does not contain a JSON object named mapwize_conf\n", conf_file);
        return -1;
    } else {
        MSG_DEBUG(LOG_INFO, "INFO~ %s does contain a JSON object named mapwize_conf, parsing mqtt parameters\n", conf_file);
    }

    str = json_object_get_string(conf_obj, "apikey");
    if (str != NULL) {
        loccfg.apikey = lgw_strdup(str);
        MSG_DEBUG(LOG_INFO, "INFO~ apikey is configured to %s\n", loccfg.apikey);
    }

    str = json_object_get_string(conf_obj, "orgid");
    if (str != NULL) {
        loccfg.orgid = lgw_strdup(str);
        MSG_DEBUG(LOG_INFO, "INFO~ orgid is configured to %s\n", loccfg.orgid);
    }

    str = json_object_get_string(conf_obj, "universesid");
    if (str != NULL) {
        loccfg.universesid = lgw_strdup(str);
        MSG_DEBUG(LOG_INFO, "INFO~ universesid is configured to %s\n", loccfg.universesid);
    }

    str = json_object_get_string(conf_obj, "placetype");
    if (str != NULL) {
        loccfg.placetype = lgw_strdup(str);
        MSG_DEBUG(LOG_INFO, "INFO~ placetype is configured to %s\n", loccfg.placetype);
    }

    conf_obj = json_object_get_object(json_value_get_object(root_val), "rssi_conf");
    if (conf_obj == NULL) {
        MSG_DEBUG(LOG_INFO, "INFO~ %s does not contain a JSON object named rssi_conf\n", conf_file);
        return -1;
    } else {
        MSG_DEBUG(LOG_INFO, "INFO~ %s does contain a JSON object named rssi_conf, parsing mqtt parameters\n", conf_file);
    }

    val = json_object_get_value(conf_obj, "rssirate");
    if (val != NULL) {
        loccfg.rssirate = (int)json_value_get_number(val);
        MSG_DEBUG(LOG_INFO, "INFO~ rssirate is configured to %d\n", loccfg.rssirate);
    } 
	
    val = json_object_get_value(conf_obj, "rssidiv");
    if (val != NULL) {
        loccfg.rssidiv = (float)json_value_get_number(val);
        MSG_DEBUG(LOG_INFO, "INFO~ rssidiv is configured to %f\n", loccfg.rssidiv);
    } 

    conf_obj = json_object_get_object(json_value_get_object(root_val), "debug_conf");
    if (conf_obj == NULL) {
        MSG_DEBUG(LOG_INFO, "INFO~ %s does not contain a JSON object named debug_conf\n", conf_file);
        return -1;
    } else {
        MSG_DEBUG(LOG_INFO, "INFO~ %s does contain a JSON object named debug_conf, parsing debug parameters\n", conf_file);
    }

    val = json_object_get_value(conf_obj, "LOG_INFO");
    if (val != NULL) {
        LOG_INFO = (int)json_value_get_number(val);
        printf("INFO~ LOG_INFO is configured to %d\n", LOG_INFO);
    } 

    val = json_object_get_value(conf_obj, "LOG_WARNING");
    if (val != NULL) {
        LOG_WARNING = (int)json_value_get_number(val);
        printf("INFO~ LOG_WARNING is configured to %d\n", LOG_WARNING);
    } 

    val = json_object_get_value(conf_obj, "LOG_ERROR");
    if (val != NULL) {
        loccfg.rssidiv = (int)json_value_get_number(val);
        printf("INFO~ LOG_ERROR is configured to %d\n", LOG_ERROR);
    } 


    /* start config topic info, may be have some topic, identify by topic_id */
    /* TODO for subscribeMany */
    /*
	serv_arry = json_object_get_array(conf_obj, "topics");
	if (serv_arry != NULL) {
        count = json_array_get_count(serv_arry);
		MSG_DEBUG(LOG_INFO, "INFO~ found %d topics", count);
        for (i = 0; i < count, i++) {
            serv_obj = json_array_get_object(serv_arry, i);
            topic_entry = lgw_malloc(sizeof(topic_s));
            topic_entry->topic_id = NULL;
            topic_entry->topic = NULL;
            str = json_object_get_string(serv_obj, "topic_id");
            if (str != NULL) {
                topic_entry->topic_id = lgw_strdup(str);
                MSG_DEBUG(LOG_INFO, "INFO~ topic id is configured to \"%s\"\n", str);
            } else {
                sprintf(tmpstr, "topic_%d", i + 1);
                topic_entry->topic_id = lgw_strdup(tmpstr);
            }

            str = json_object_get_string(serv_obj, "topic");
            if (str != NULL) {
                topic_entry->topic = lgw_strdup(str);
			    MSG_DEBUG(LOG_INFO, "INFO~ topic content is configured to \"%s\"\n", str);
            } else {
                sprintf(tmpstr, "/topic_%d/*", i + 1);    // default topic for mqtt suscribe
                topic_entry->topic = lgw_strdup(tmpstr);
            }
            LGW_LIST_INSERT_TAIL(loccfg.topic_list, topic_entry);
        }

    } else 
        MSG_DEBUG(LOG_WARNING, "WARNING: No topic offer.\n");
    */
	
    /* free JSON parsing data structure */
    json_value_free(root_val);
    return 0;
}

static void connlost(void *context, char *cause)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	int rc;

	printf("\nConnection lost\n");
	if (cause)
		printf("     cause: %s\n", cause);

	printf("Reconnecting\n");
	conn_opts.keepAliveInterval = 20;
	conn_opts.cleansession = 1;
	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %d\n", rc);
		finished = 1;
	}
}


static int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    payload_s* payload_entry = NULL;

    MSG_DEBUG(LOG_INFO, "MDEBUG~ message arrived\n");
    MSG_DEBUG(LOG_INFO, "DEBUG~  topic: %s\n", topicName);
    MSG_DEBUG(LOG_INFO, "DEBUG~  message: %.*s\n", message->payloadlen, (char*)message->payload);

    payload_entry = lgw_malloc(sizeof(payload_s));
    payload_entry->type = loccfg.serv_type;
    payload_entry->len = message->payloadlen;
    payload_entry->content = lgw_strdup((char*)message->payload);

    LGW_LIST_LOCK(&payload_list);
    LGW_LIST_INSERT_TAIL(&payload_list, payload_entry, list);
    LGW_LIST_UNLOCK(&payload_list);

    sem_post(&parse_payload_sem);

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

static void onDisconnectFailure(void* context, MQTTAsync_failureData* response)
{
	MSG_DEBUG(LOG_INFO, "DEBUG~ Disconnect failed, rc %s\n", MQTTAsync_strerror(response->code));
	disc_finished = 1;
}

static void onDisconnect(void* context, MQTTAsync_successData* response)
{
	MSG_DEBUG(LOG_INFO, "DEBUG~ Successful disconnection\n");
	disc_finished = 1;
}

static void onSubscribe(void* context, MQTTAsync_successData* response)
{
	MSG_DEBUG(LOG_INFO, "DEBUG~ Subscribe succeeded\n");
	subscribed = 1;
}

static void onSubscribeFailure(void* context, MQTTAsync_failureData* response)
{
	MSG_DEBUG(LOG_INFO, "DEBUG~ Subscribe failed, rc %s\n", MQTTAsync_strerror(response->code));
	finished = 1;
}


static void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	MSG_DEBUG(LOG_INFO, "DEBUG~ Connect failed, rc %s\n", MQTTAsync_strerror(response->code));
	finished = 1;
}


static void onConnect(void* context, MQTTAsync_successData* response)
{
	MQTTAsync client = (MQTTAsync)context;
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
	int rc;

	MSG_DEBUG(LOG_INFO, "DEBUG~ Successful connection\n");

    MSG_DEBUG(LOG_INFO, "DEBUG~ Subscribing to topic %s for client %s using QoS%d\n\n", loccfg.topic, loccfg.clientid, loccfg.qos);
    opts.onSuccess = onSubscribe;
    opts.onFailure = onSubscribeFailure;
    opts.context = client;
    if ((rc = MQTTAsync_subscribe(client, loccfg.topic, loccfg.qos, &opts)) != MQTTASYNC_SUCCESS)
    {
        printf("Failed to start subscribe, return code %d\n", rc);
        finished = 1;
    }
}


/* -------------------------------------------------------------------------- */
/* --- MAIN FUNCTION -------------------------------------------------------- */
int main(int argc, char* argv[])
{
    struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */

    /* configuration file related */
    char* conf_fname= "/etc/location_conf.json"; /* contain global (typ. network-wide) configuration */

    curlstr_s* curl_write_data;

    char* url = NULL;

	MQTTAsync client;
	MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
	MQTTAsync_disconnectOptions disc_opts = MQTTAsync_disconnectOptions_initializer;
	int rc;
	int ch;

    pthread_t thrid_parse_payload;
    pthread_t thrid_create_place;

    /* configure signal handling */
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = sig_handler;
    sigaction(SIGQUIT, &sigact, NULL); /* Ctrl-\ */
    sigaction(SIGINT, &sigact, NULL); /* Ctrl-C */
    sigaction(SIGTERM, &sigact, NULL); /* default "kill" command */

    sem_init(&parse_payload_sem, 0, 0);

    sem_init(&parse_inode_sem, 0, 0);

    printf("DEBUG~ starting location service!\n");

    if (access(conf_fname, R_OK) != 0) {
        printf("ERROR~ can't read configure file: %s, exit!\n", conf_fname);
        exit(EXIT_FAILURE);
    }

    if (parse_serv_cfg(conf_fname)) {
        printf("ERROR~ can't parse serv config, EXIT ERROR!\n");
		exit(EXIT_FAILURE);
    }

    MSG_DEBUG(LOG_INFO, "DEBUG~ getting placetype...!\n");

    curl_write_data = init_curl_write_data();

    mapwize_get_placetype(loccfg.apikey, loccfg.orgid, (void*)curl_write_data);  //sercfg.placetypeid 

    get_placetype(curl_write_data);

    lgw_free(curl_write_data->ptr);
    lgw_free(curl_write_data);

    curl_write_data = init_curl_write_data();

    mapwize_get_beacons(loccfg.apikey, (void*)curl_write_data);  // ibeacon_list  

    get_beacons(curl_write_data);

    lgw_free(curl_write_data->ptr);
    lgw_free(curl_write_data);

    MSG_DEBUG(LOG_INFO, "DEBUG~ getting beacons Done!\n");

    if (NULL != loccfg.connection)
        url = loccfg.connection;
    else {
        url = (char*)lgw_malloc(DEFAULT_URL_LEN * sizeof(char));
        snprintf(url, DEFAULT_URL_LEN, "tcp://%s:%d", loccfg.servaddr, loccfg.servport);
    }

    MSG_DEBUG(LOG_INFO, "DEBUG~ create parse payload thread...\n");
    if (lgw_pthread_create(&thrid_parse_payload, NULL, (void *(*)(void *))thread_parse_payload, NULL))
        MSG_DEBUG(LOG_INFO, "DEBUG~ ERROR, Can't create thread of parse payload");

    MSG_DEBUG(LOG_INFO, "DEBUG~ create create place thread...\n");
    if (lgw_pthread_create(&thrid_create_place, NULL, (void *(*)(void *))thread_create_place, NULL))
        MSG_DEBUG(LOG_INFO, "DEBUG~ ERROR, Can't create thread of create place");

    MSG_DEBUG(LOG_INFO, "DEBUG~ create mqtt connection: %s\n", url);

	if ((rc = MQTTAsync_create(&client, url, loccfg.clientid, MQTTCLIENT_PERSISTENCE_NONE, NULL))
			!= MQTTASYNC_SUCCESS)
	{
		printf("Failed to create client, return code %s\n", MQTTAsync_strerror(rc));
		rc = EXIT_FAILURE;
		goto destroy_exit;
	}

    MSG_DEBUG(LOG_INFO, "DEBUG~ create mqtt callbacks\n");

	if ((rc = MQTTAsync_setCallbacks(client, client, connlost, msgarrvd, NULL)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to set callbacks, return code %s\n", MQTTAsync_strerror(rc));
		rc = EXIT_FAILURE;
		goto destroy_exit;
	}

	conn_opts.keepAliveInterval = DEFUALT_KEEPALIVE;
	conn_opts.cleansession = 1;
    conn_opts.username = loccfg.username;
    conn_opts.password = loccfg.password;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = client;

    MSG_DEBUG(LOG_INFO, "DEBUG~ staring connect mqtt server\n");

	if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start connect, return code %s\n", MQTTAsync_strerror(rc));
		rc = EXIT_FAILURE;
		goto destroy_exit;
	}

	while (!subscribed && !finished) {
	    usleep(TIMEOUT);
    }

	if (finished)
		goto destroy_exit;

    MSG_DEBUG(LOG_INFO, "DEBUG~  subscribing mqtt message\n");

    while (!exit_sig && !quit_sig) {  // main thread for subscribe
	}  

	disc_opts.onSuccess = onDisconnect;
	disc_opts.onFailure = onDisconnectFailure;
	if ((rc = MQTTAsync_disconnect(client, &disc_opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start disconnect, return code %s\n", MQTTAsync_strerror(rc));
		rc = EXIT_FAILURE;
		goto destroy_exit;
	}
 	while (!disc_finished)
 	{
		usleep(10000L);
 	}

    sem_post(&parse_payload_sem);
    sem_post(&parse_inode_sem);
    pthread_join(thrid_parse_payload, NULL);
    pthread_join(thrid_create_place, NULL);

destroy_exit:
	MQTTAsync_destroy(&client);
    free_cfg_entry(&loccfg);
 	return rc;
}


static void thread_parse_payload() 
{
    /* JSON parsing variables */
    JSON_Value *root_val;
    JSON_Object *mqtt_obj = NULL;
    JSON_Object *payload_obj = NULL;
    JSON_Value *val = NULL; /* needed to detect the absence of some fields */
    const char *str; /* pointer to sub-strings in the JSON data */

    payload_s* payload_entry = NULL;
    inode_s* inode_entry = NULL;

    bool parse_ok;

    while (!exit_sig && !quit_sig) {
        lgw_wait_sem(&parse_payload_sem, DEFAULT_LOOP_MS); // every 10 seconds

        MSG_DEBUG(LOG_INFO, "DEBUG~ parse payload thread trigger parse...\n");

        LGW_LIST_LOCK(&payload_list);
        LGW_LIST_TRAVERSE_SAFE_BEGIN(&payload_list, payload_entry, list) {
            if (payload_entry) {
                LGW_LIST_REMOVE_CURRENT(list);
                break;
            }
        }
        LGW_LIST_TRAVERSE_SAFE_END;
        LGW_LIST_UNLOCK(&payload_list);

        if (payload_entry == NULL) continue;

        MSG_DEBUG(LOG_INFO, "DEBUG~ payload(%d): %s\n",
                payload_entry->type,
                payload_entry->content);
        
        inode_entry = (inode_s*)lgw_malloc(sizeof(inode_s));

        inode_entry->type = iBEACON;

        parse_ok = true;

        switch (payload_entry->type) {
            case TTN:
                root_val = json_parse_string_with_comments((const char*)payload_entry->content);
                if (root_val == NULL) {
                    MSG_DEBUG(LOG_WARNING, "WARNING~ receive invalid JSON,  aborted\n");
                    parse_ok = false;
                    break;
                }
                str = json_object_get_string(json_value_get_object(root_val), "dev_id");
                if (str != NULL) {
                    inode_entry->devid = lgw_strdup(str);
                } else {
                    MSG_DEBUG(LOG_WARNING, "WARNING~ can't get device id \n");
                    parse_ok = false;
                    break;
                }
                str = json_object_get_string(json_value_get_object(root_val), "hardware_serial");
                if (str != NULL) {
                    inode_entry->deveui = lgw_strdup(str);
                } else {
                    MSG_DEBUG(LOG_WARNING, "WARNING~ can't get deveui id \n");
                    parse_ok = false;
                    break;
                }
                payload_obj = json_object_get_object(json_value_get_object(root_val), "payload_fields");
                if (payload_obj == NULL) {
                    MSG_DEBUG(LOG_INFO, "INFO~ does not contain a JSON object named payload_fields\n");
                    parse_ok = false;
                    break;
                }

                str = json_object_get_string(payload_obj, "UUID");
                if (str != NULL) {
                    inode_entry->uuid = lgw_strdup(str);
                } else {
                    MSG_DEBUG(LOG_WARNING, "WARNING~ can't get uuid, drop the payload\n");
                    parse_ok = false;
                    break;
                }

                val = json_object_get_value(payload_obj, "MAJOR");
                if (str != NULL) {
                    inode_entry->major = (int)json_value_get_number(val);
                } else {
                    MSG_DEBUG(LOG_WARNING, "WARNING~ can't get major id, drop the payload\n");
                    parse_ok = false;
                    break;
                }

                val = json_object_get_value(payload_obj, "MINOR");
                if (val != NULL) {
                    inode_entry->minor = (int)json_value_get_number(val);
                } else {
                    MSG_DEBUG(LOG_WARNING, "WARNING~ can't get minor id, drop the payload\n");
                    parse_ok = false;
                    break;
                }

                val = json_object_get_value(payload_obj, "RSSI");
                if (val != NULL) {
                    inode_entry->rssi = (int)json_value_get_number(val);
                    inode_entry->dist = calc_dist_byrssi(inode_entry->rssi, loccfg.rssirate, loccfg.rssidiv);
                    MSG_DEBUG(LOG_INFO, "INFO~ get rssi from message %d\n", inode_entry->rssi);
                } else {
                    MSG_DEBUG(LOG_WARNING, "WARNING~ can't get rssi, drop the payload\n");
                    parse_ok = false;
                    break;
                }

                break;
            default:
                break;
        }

        if (parse_ok) {
            LGW_LIST_LOCK(&inode_list);
            LGW_LIST_INSERT_TAIL(&inode_list, inode_entry, list);
            sem_post(&parse_inode_sem);
            LGW_LIST_UNLOCK(&inode_list);
        } else {
            MSG_DEBUG(LOG_INFO, "DEBUG~ Failed to parse a payload, skip to next!\n");
            free_inode_entry(inode_entry);
        }

        lgw_free(payload_entry->content);
        lgw_free(payload_entry);

        json_value_free(root_val);
    }
}

static void thread_create_place() 
{

    char place_id[25] = {0};
    char place_data[1024] = {0};

    inode_s* inode_entry = NULL;
    ibeacon_s* ibeacon_entry = NULL;

    while (!exit_sig && !quit_sig) {
        lgw_wait_sem(&parse_inode_sem, DEFAULT_LOOP_MS); // every 10 seconds
        MSG_DEBUG(LOG_INFO, "DEBUG~  Trigger create place thread...\n");
        LGW_LIST_LOCK(&inode_list);
        LGW_LIST_TRAVERSE_SAFE_BEGIN(&inode_list, inode_entry, list) {
            if (inode_entry) {
                LGW_LIST_REMOVE_CURRENT(list);
                break;
            }
        }
        LGW_LIST_TRAVERSE_SAFE_END;
        LGW_LIST_UNLOCK(&inode_list);

        if (inode_entry == NULL) continue;

        MSG_DEBUG(LOG_INFO, "DEBUG~ CreateplaceData deveui = %s, devid = %s \n", inode_entry->deveui, inode_entry->devid);

        LGW_LIST_TRAVERSE(&ibeacon_list, ibeacon_entry, list) {
            if (inode_entry->minor != ibeacon_entry->minor) {
                continue;
            } else if (inode_entry->major != ibeacon_entry->major) {
                continue;
            } else if (strncmp(inode_entry->uuid, ibeacon_entry->uuid, 12)) {   // compare tail of uuid (12 char)
                continue;
            } else {
                snprintf(place_id, sizeof(place_id), "7f9abcd9%s", inode_entry->deveui);
                snprintf(place_data, sizeof(place_data), 
                        "{\"name\":\"%s\",\"description\":\"moveable place point (%s)\",\"floor\":%d,\"geometry\":{\"type\":\"Point\",\"coordinates\":[%.15lf,%.15lf]},\"_id\":\"%s\", \"universes\":\"%s\", \"placeTypeId\":\"%s\",\"isPublished\":true,\"isSearchable\":true,\"isVisible\":true,\"isClickable\":true,\"searchKeywords\":\"%s\", \"translations\":[{\"title\":\"%s\",\"language\":\"en\"}], \"venueId\":\"%s\",\"owner\":\"%s\"}",
                        inode_entry->devid, inode_entry->deveui, ibeacon_entry->floor, // floor
                        ibeacon_entry->gps.lon, ibeacon_entry->gps.lat,        // location
                        place_id, loccfg.universesid,loccfg.placetypeid,       //placetypeid
                        inode_entry->devid, inode_entry->devid,                //title
                        ibeacon_entry->venueid, ibeacon_entry->orgid);
                MSG_DEBUG(LOG_INFO, "DEBUG~ CreateplaceData: %s \n", place_data);
                mapwize_del_places(loccfg.apikey, place_id);    // delete the duplicate place if exist
                mapwize_create_place(loccfg.apikey, place_data);
                break;
            }
        }

        free_inode_entry(inode_entry);
    }
}


/*!
 * \brief calc distance between node and gw, formula: d = 10^((abs(RSSI) - A) / (10 * n)) 
 * \param rate the rssi of the distance of 1 meter, default (45-49)?
 * \param div attenuation, default 2.0?
 * \retval distance
 */

static float calc_dist_byrssi(int rssi, int rate, float div)
{
    int irssi = abs(rssi);
    float power = (irssi - rate)/(10 * 2.0);
    return pow(10, power);
}

static void free_inode_entry(inode_s* node)
{
    lgw_free(node->devid);
    lgw_free(node->deveui);
    lgw_free(node->uuid);
    lgw_free(node);
}

static void free_cfg_entry(loccfg_s* cfg)
{
    lgw_free(cfg->servaddr);
    lgw_free(cfg->clientid);
    lgw_free(cfg->username);
    lgw_free(cfg->password);
    lgw_free(cfg->topic);
    lgw_free(cfg->connection);
    lgw_free(cfg->apikey);
    lgw_free(cfg->venueid);
    lgw_free(cfg->orgid);
    lgw_free(cfg->universesid);
    lgw_free(cfg->placetype);
    lgw_free(cfg->placetypeid);
}

static int get_placetype(curlstr_s* cstr)
{
    JSON_Value *root_val;
    JSON_Object *obj = NULL;
    JSON_Array *root_array = NULL;
    const char *str;

    int i, count;
    bool get_object;

    MSG_DEBUG(LOG_INFO, "DEBUG~ %s\n", cstr->ptr);

    root_val = json_parse_string_with_comments(cstr->ptr);
    if (root_val == NULL) {
        MSG_DEBUG(LOG_ERROR, "ERROR~ \n %s \n is not a valid JSON string\n", cstr->ptr);
        free(cstr->ptr);
        return -1;
    }

    root_array = json_value_get_array(root_val);
    if (NULL == root_array) {
        json_value_free(root_val);
        return -1;
    }

    count = json_array_get_count(root_array);

    for (i = 0; i < count; i++) {
        get_object = false;

        obj = json_array_get_object(root_array, i);

        str = json_object_get_string(obj, "name");

        if (str != NULL && !strcmp(str, loccfg.placetype)) {   // is placetype name 
            str = json_object_get_string(obj, "_id");         // get placetype id
            if (str != NULL) {
                loccfg.placetypeid = lgw_strdup(str);
                MSG_DEBUG(LOG_INFO, "DEBUG~ configure placetypeid to %s \n", str);
            } 
            break;
        }
    }

    if (NULL == loccfg.placetypeid) {
        MSG_DEBUG(LOG_INFO, "DEBUG~ placetypeid is NULL, Must configure a currect placetypeid\n" );
    }

    json_value_free(root_val);

    return 0;
}

static int get_beacons(curlstr_s* cstr)
{
    int i, count;

    JSON_Value *root_val;
    JSON_Object *iobj = NULL;
    JSON_Object *obj = NULL;
    JSON_Array *root_array = NULL;
    JSON_Value *val = NULL; /* needed to detect the absence of some fields */
    const char *str;
    
    bool getbeacon = false;

    ibeacon_s* ibeacon_entry = NULL;

    MSG_DEBUG(LOG_INFO, "DEBUG~ %s\n", cstr->ptr);

    root_val = json_parse_string_with_comments(cstr->ptr);
    if (root_val == NULL) {
        MSG_DEBUG(LOG_ERROR, "ERROR~ \n %s \n is not a valid JSON string\n", cstr->ptr);
        free(cstr->ptr);
        return -1;
    }

    root_array = json_value_get_array(root_val);
    if (NULL == root_array) {
        json_value_free(root_val);
        free(cstr->ptr);
        return -1;
    }

    count = json_array_get_count(root_array);
    
    for (i = 0; i < count; i++) {

        iobj = json_array_get_object(root_array, i);

        str = json_object_get_string(iobj, "type");
        if (str != NULL && !strcmp("ibeacon", str)) {
            getbeacon = true;
        } else {
            MSG_DEBUG(LOG_INFO, "DEBUG~ current beacon type is %s\n", str);
            continue;  // type is not ibeacon, skip ...
        }

        ibeacon_entry = (ibeacon_s*)lgw_malloc(sizeof(ibeacon_s));

        //get venueId
        str = json_object_get_string(iobj, "_id");
        if (str != NULL) {
            ibeacon_entry->id = lgw_strdup(str);
            MSG_DEBUG(LOG_INFO, "DEBUG~ Id set to %s\n", str);
        } else {
            getbeacon = false;
        }

        //get venueId
        str = json_object_get_string(iobj, "venueId");
        if (str != NULL) {
            ibeacon_entry->venueid = lgw_strdup(str);
            MSG_DEBUG(LOG_INFO, "DEBUG~ venueId set to %s\n", str);
        } else {
            getbeacon = false;
        }

        // get owner, orgid
        str = json_object_get_string(iobj, "owner");
        if (str != NULL && getbeacon) {
            ibeacon_entry->orgid = lgw_strdup(str);
            MSG_DEBUG(LOG_INFO, "DEBUG~ orgId set to %s\n", str);
        } else {
            getbeacon = false;
        }

        // get floor
        val = json_object_get_value(iobj, "floor");
        if (getbeacon && (json_value_get_type(val) == JSONNumber)) {
            ibeacon_entry->floor = (int)json_value_get_number(val);
            MSG_DEBUG(LOG_INFO, "DEBUG~ floor set to %d\n", ibeacon_entry->floor);
        } else {
            getbeacon = false;
        }

        // get owner, orgid
        obj = json_object_get_object(iobj, "properties");
        if (obj != NULL && getbeacon) {
            str = json_object_get_string(obj, "uuid");
            if (str != NULL) {
                ibeacon_entry->uuid = lgw_strdup(str + 24);
                MSG_DEBUG(LOG_INFO, "DEBUG~ uuid set to %s\n", ibeacon_entry->uuid);  // the last 12 characters
            } else {
                getbeacon = false;
            }
            str = json_object_get_string(obj, "major");
            if (str != NULL && getbeacon) {
                ibeacon_entry->major = atoi(str);
                MSG_DEBUG(LOG_INFO, "DEBUG~ major set to %d\n", ibeacon_entry->major);
            } else {
                getbeacon = false;
            }
            str = json_object_get_string(obj, "minor");
            if (str != NULL && getbeacon) {
                ibeacon_entry->minor = atoi(str);
                MSG_DEBUG(LOG_INFO, "DEBUG~ minor set to %d\n", ibeacon_entry->minor);
            } else {
                getbeacon = false;
            }
        } else {
            getbeacon = false;
        }

        obj = json_object_get_object(iobj, "location");
        if (obj != NULL && getbeacon) {
            val = json_object_get_value(obj, "lat");
            if (getbeacon && (json_value_get_type(val) == JSONNumber)) {
                ibeacon_entry->gps.lat = (double)json_value_get_number(val);
                MSG_DEBUG(LOG_INFO, "DEBUG~ lat set to %f\n", ibeacon_entry->gps.lat);
            } else {
                getbeacon = false;
            }

            val = json_object_get_value(obj, "lon");
            if (getbeacon && (json_value_get_type(val) == JSONNumber)) {
                ibeacon_entry->gps.lon = (double)json_value_get_number(val);
                MSG_DEBUG(LOG_INFO, "DEBUG~ lon set to %f\n", ibeacon_entry->gps.lon);
            } else {
                getbeacon = false;
            }
        } else {
            getbeacon = false;
        }

        if (!getbeacon) {
            MSG_DEBUG(LOG_INFO, "DEBUG~ Getbeacon error skip!\n");
            lgw_free(ibeacon_entry->id);
            lgw_free(ibeacon_entry->venueid);
            lgw_free(ibeacon_entry->orgid);
            lgw_free(ibeacon_entry->uuid);
            lgw_free(ibeacon_entry);
            continue;
        }

        // getbaecon true 
        LGW_LIST_INSERT_TAIL(&ibeacon_list, ibeacon_entry, list);

    }

    json_value_free(root_val);

    return 0;

}
