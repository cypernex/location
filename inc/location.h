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
 * \brief lora location service definition
 *
 */

#ifndef _DR_LOCATION_H_
#define _DR_LOCATION_H_

#include <stdint.h>

/*!
 * \brief mqtt server type such as TTN 
 */
typedef enum {
    UNKNOWN,
    TTN = 1
} serv_type_e;

/*!
 * \brief location method type such as beacon 
 */
typedef enum {
    iBEACON = 1,
    RSSI,
    GPS,
    OTHER
} loc_method_e;

/*!
 * \brief location map type such as indoor 
 */
typedef enum {
    MAPWIZE,
    TRACCAR
} loc_map_e;

/*!
 * \brief struct of topic struct 
 */
typedef struct _topic_s {
    LGW_LIST_ENTRY(_topic_s) list;
    char* topic_id;
    char* topic;
} topic_s;

/*!
 * \brief struct of topic head
 */
//LGW_LIST_HEAD_NOLOCK(topic_list, _topic_s); 


/*!
 * \brief struct of mqtt payload
 */
typedef struct _payload_s {
    LGW_LIST_ENTRY(_payload_s) list;
    serv_type_e type;
    int len;
    char* content;
} payload_s;

/*!
 * \brief struct of ibeacon node payload
 * \para reqbody for outdoor location, generate for loracloud api3, format: { "gateways": ARRAY, "frames": ARRAY }
 */
typedef struct _inode_s {
    LGW_LIST_ENTRY(_inode_s) list;
    loc_method_e type;
    char* reqbody;  
    char* devid;
    char* deveui;
    char* uuid;
    int major;
    int minor;
    int rssi;
    float dist;
} inode_s;

typedef struct _coornode_s {
    LGW_LIST_ENTRY(_coornode_s) list;
    char* devid;
    char* deveui;
    char* coordata;
} coornode_s;

/*!
 * \brief struct of 
 */
typedef struct {
    double lat;
    double lon;
    float alt;
} gps_s;

/*!
 * \brief struct of 
 */
typedef struct _ibeacon_s {
    LGW_LIST_ENTRY(_ibeacon_s) list;
    char* id;
    char* venueid;
    char* orgid;
    char* uuid;
    int major;
    int minor;
    int floor;
    gps_s gps;
} ibeacon_s;

/*!
 * \brief struct of 
 */
typedef struct {
    char* name;
    char* placeid;
    char* placetypeid;
    char* venueid;
    char* orgid;
    char* keywork;
} place_s;

/*!
 * \brief struct of mqtt configure 
 */
typedef struct {
    serv_type_e serv_type;
    loc_method_e locmd_type;
    loc_map_e  locmap;
    bool isindoor;

    //configure of mqtt server;
    char* servaddr;
    uint16_t servport;
    char* clientid;
    int qos;
    uint32_t timeout;
    char* username;
    char* password;
    char* topic;
    char* connection;

    //configure of mapwize server;
    char* apikey;
    char* venueid;
    char* orgid;
    char* universesid;
    char* placetype;
    char* placetypeid;

    //configure of loracloud
    char* token;

    //configure of distance
    int rssirate;
    float rssidiv;
} loccfg_s;

#define LOCCFG_INIT { TTN, iBEACON, MAPWIZE, true, NULL, 1833, NULL, 1, 1000, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 45, 2.0 }

#endif       // _DR_LOCATION_H_


