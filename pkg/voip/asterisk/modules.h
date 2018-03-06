/****************************************************************************
 *
 * rg/pkg/voip/asterisk/modules.h * 
 * 
 * Modifications by Jungo Ltd. Copyright (C) 2008 Jungo Ltd.  
 * All Rights reserved. 
 * 
 * This program is free software; 
 * you can redistribute it and/or modify it under 
 * the terms of the GNU General Public License version 2 as published by 
 * the Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty 
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
 * See the GNU General Public License v2 for more details.
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program. 
 * If not, write to the Free Software Foundation, 
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA, 
 * or contact Jungo Ltd. 
 * at http://www.jungo.com/openrg/opensource_ack.html
 * 
 * 
 * 
 */

#ifndef _MODULES_H_
#define _MODULES_H_

#define MODULE_FUNCTIONS_NO_RELOAD(name) \
    char *name##_description(void); \
    char *name##_key(void); \
    int name##_load_module(void); \
    int name##_unload_module(void); \
    int name##_usecount(void);

#define MODULE_FUNCTIONS_WITH_RELOAD(name) \
    char *name##_description(void); \
    char *name##_key(void); \
    int name##_reload(void); \
    int name##_load_module(void); \
    int name##_unload_module(void); \
    int name##_usecount(void);

#define MODULE_FUNCTIONS_WITH_RELOAD_IF_CHANGED(name) \
    char *name##_description(void); \
    char *name##_key(void); \
    int name##_reload(void); \
    int name##_reload_if_changed(void); \
    int name##_load_module(void); \
    int name##_unload_module(void); \
    int name##_usecount(void);

#define M_WITH_RELOAD_IF_CHANGED(name) {#name, name##_load_module, name##_unload_module, name##_reload, name##_reload_if_changed, name##_description, name##_usecount, name##_key}

#define M_WITH_RELOAD(name) {#name, name##_load_module, name##_unload_module, name##_reload, NULL, name##_description, name##_usecount, name##_key}

#define M_NO_RELOAD(name) {#name, name##_load_module, name##_unload_module, NULL, NULL, name##_description, name##_usecount, name##_key}

#include <prototypes.h>

typedef struct {
    char *name;
    int (*load_module)(void);
    int (*unload_module)(void);
    int (*reload)(void);
    int (*reload_if_changed)(void);
    char *(*description)(void);
    int (*usecount)(void);
    char *(*key)(void);
} module_functions_t;

module_functions_t modules[] = {
#include <ast_modules.h>
    {NULL}
};

#endif

