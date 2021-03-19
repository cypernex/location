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

#ifndef _LGW_MAPWIZE_H
#define _LGW_MAPWIZE_H

/*!
 * \brief struct of curl callback writedata 
 */
typedef struct {
  	char* ptr;
  	size_t len;
} curlstr_s;

/*!
 * \brief initialize curl write data struct 
 */
curlstr_s* init_curl_write_data();

/*!
 * \brief allows you to sign in using your email and password
 */
int mapwize_signin(char* apikey, char* email, char* passwd);

/*!
 * \brief 
 */
int mapwize_get_placetype(char* apikey, char* orgid, void* data);

/*!
 * \brief 
 */
int mapwize_create_place(char* apikey, char* data);

/*!
 * \brief 
 */
int mapwize_del_places(char* apikey, char* placeid);

/*!
 * \brief 
 */
int mapwize_create_beacons(char* apikey, char* data);

/*!
 * \brief 
 */
int mapwize_get_beacons(char* apikey, void* writedata);

#endif
