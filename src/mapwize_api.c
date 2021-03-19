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
#include <stdlib.h>
#include <curl/curl.h>

#include "utilities.h"
#include "mapwize_api.h"

static size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* s)
{
    curlstr_s* cstr = (curlstr_s*)s;

    size_t new_len = cstr->len + size*nmemb;

    cstr->ptr = lgw_realloc(cstr->ptr, new_len+1);
    if (cstr->ptr == NULL) {
        fprintf(stderr, "crul callback function realloc() failed\n");
        return cstr->len;
    }

    memcpy(cstr->ptr + cstr->len, ptr, size*nmemb);
    cstr->ptr[new_len] = '\0';
    cstr->len = new_len;

    MSG_DEBUG(LOG_INFO, "DEBUG~ %s\n", cstr->ptr);

    return size*nmemb;
}

curlstr_s* init_curl_write_data() 
{
    curlstr_s* s = lgw_malloc(sizeof(curlstr_s));
    if (s == NULL) {
      fprintf(stderr, "malloc() failed\n");
      exit(EXIT_FAILURE);
    }
    s->len = 0;
    s->ptr = lgw_malloc(s->len+1);
    if (s->ptr == NULL) {
      fprintf(stderr, "malloc() failed\n");
      exit(EXIT_FAILURE);
    }
    s->ptr[0] = '\0';
    return s;
}

int mapwize_signin(char* apikey, char* email, char* passwd) 
{
    char url[96] = {0};
    char data[96] = {0};

    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();

    snprintf(url, sizeof(url), "https://api.mapwize.io/v1/auth/signin?api_key=%s", apikey);
    snprintf(data, sizeof(data), "{\"email\":\"%s\", \"password\":\"%s\"}", email, passwd);

    if(curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        res = curl_easy_perform(curl);
    }
    curl_easy_cleanup(curl);
    return (int)res;
}


int mapwize_get_placetype(char* apikey, char* orgid, void* data)
{
    CURL *curl;
    CURLcode res;

    char url[128] = {0};
    snprintf(url, sizeof(url), "https://api.mapwize.io/v1/placeTypes?api_key=%s&organizationId=%s", apikey, orgid);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, data);
        struct curl_slist *headers = NULL;
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        res = curl_easy_perform(curl);
    }
    curl_easy_cleanup(curl);
    return (int)res;
}

int mapwize_create_place(char* apikey, char* data)
{
    CURL *curl;
    CURLcode res;
    char errbuf[CURL_ERROR_SIZE];
    char url[96] = {0};

    snprintf(url, sizeof(url), "https://api.mapwize.io/v1/places?api_key=%s", apikey);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        //const char *data = "{\"name\":\"Office\",\"description\":\"Room description\",\"floor\":0,\"geometry\":{\"type\":\"Point\",\"coordinates\":[-9.137969613075256,38.713773333472425]},\"placeTypeId\":\"{{placeTypeId}}\",\"isPublished\":true,\"isSearchable\":true,\"isVisible\":true,\"isClickable\":true,\"style\":{\"markerUrl\":\"https://mapwize.blob.core.windows.net/placetypes/30/room.png\",\"markerDisplay\":true,\"strokeColor\":\"#711083\",\"strokeOpacity\":0.5,\"strokeWidth\":\"1\",\"fillColor\":\"#f23196\",\"fillOpacity\":0.5,\"labelBackgroundColor\":\"#000\",\"labelBackgroundOpacity\":1},\"searchKeywords\":\"Mr X's office,X's office\",\"translations\":[{\"title\":\"Bureau de Mr X\",\"language\":\"fr\"},{\"title\":\"Mr X's office\",\"language\":\"en\"}],\"data\":{\"ID\":\"XBCDJJD\"},\"venueId\":\"{{venueId}}\",\"owner\":\"{{organizationId}}\"}";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        errbuf[0] = '\0';
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            MSG_DEBUG(LOG_WARNING, "WARNING~, create place error: %s\n", strlen(errbuf) > 1 ? errbuf : curl_easy_strerror(res));
        }
    }
    curl_easy_cleanup(curl);
    return (int)res;
}

int mapwize_del_places(char* apikey, char* placeid) 
{
    CURL *curl;
    CURLcode res;

    char url[128] = {0};
    snprintf(url, sizeof(url), "https://api.mapwize.io/v1/places/%s?api_key=%s", placeid, apikey);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        res = curl_easy_perform(curl);
    }
    curl_easy_cleanup(curl);
    return (int)res;
}

int mapwize_create_beacons(char* apikey, char* data)
{
    CURL *curl;
    CURLcode res;

    char url[96] = {0};
    snprintf(url, sizeof(url), "https://api.mapwize.io/v1/beacons?api_key=%s", apikey);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        //const char *data = "{\"name\":\"iBeacon1\",\"type\":\"ibeacon\",\"location\":{\"lat\":38.71404746390113,\"lon\":-9.140428906009676},\"floor\":1,\"properties\":{\"uuid\":\"D94194BD-105B-4366-9785-271B25AD26C1\",\"major\":\"0\",\"minor\":\"0\"},\"venueId\":\"{{venueId}}\",\"owner\":\"{{organizationId}}\",\"isPublished\":true}";
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        res = curl_easy_perform(curl);
    }
    curl_easy_cleanup(curl);
    return (int)res;
}

int mapwize_get_beacons(char* apikey, void* writedata)
{
    CURL *curl;
    CURLcode res;

    char url[96] = {0};
    snprintf(url, sizeof(url), "https://api.mapwize.io/v1/beacons?api_key=%s&isPublished=all", apikey);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        struct curl_slist *headers = NULL;
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, writedata);
        res = curl_easy_perform(curl);
    }
    curl_easy_cleanup(curl);
    return (int)res;
}

