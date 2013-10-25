/*****************************************************************************
 * srpos.c : Save and restore position of the last played files
 *****************************************************************************
 * Copyright (C) 2012-2013
 * $Id:
 *
 * Authors: Artem Senichev <artemsen@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc_common.h>
#include <vlc_plugin.h>
#include "vlc_interface.h" /* This file not included to VLC SDK */
#include <vlc_playlist.h>

/*****************************************************************************
 * Internal state for an instance of the module
 *****************************************************************************/
typedef struct srpos_list_t srpos_list_t;
struct srpos_list_t
{
    char *psz_uri;         /* File uri */
    double f_position;     /* Last position in file */
    bool b_used;           /* Flag signaled that position has been already set */
    srpos_list_t *p_next;  /* Pointer to next item (list realisation) */
};
struct intf_sys_t
{
    srpos_list_t* p_list;  /* File position info list */
};

/*****************************************************************************
 * Forward declarations
 *****************************************************************************/
static int  Open( vlc_object_t * );
static void Close( vlc_object_t * );
static int  ItemChange( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t, void * );
static FILE* GetSettingsFile( const bool );
static srpos_list_t* GetPositionInfo( intf_sys_t *, const char * );
static void SetPositionInfo( intf_sys_t *, const char *, double );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin()
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_shortname( MODULE_STRING )
    set_description( "Save/restore position of the last played files" )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end()

/*****************************************************************************
 * Open: initialize interface
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    char *psz_sfile_data = NULL;
    char *psz_sfile_uri = NULL;
    srpos_list_t *p_prev = NULL;
    srpos_list_t *p_curr = NULL;
    FILE *p_sfile = NULL;

    /* Allocate memory */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
        return VLC_ENOMEM;
    p_intf->p_sys->p_list = NULL;

    /* Load files position info */
    p_sfile = GetSettingsFile( true );
    if( p_sfile )
    {
        psz_sfile_data = (char *)malloc( 1024 );
        if( psz_sfile_data )
        {
            memset( psz_sfile_data, 0, 1024 );
            while ( fgets( psz_sfile_data, 1024 - 1, p_sfile ) != NULL && *psz_sfile_data != 0)
            {
                p_curr = (srpos_list_t*)malloc( sizeof( srpos_list_t ) );
                if ( p_curr != NULL )
                {
                    p_curr->b_used = false;
                    p_curr->p_next = NULL;
                    p_curr->f_position = atof( psz_sfile_data );
                    if( p_curr->f_position < 0.0 || p_curr->f_position > 1.0 )
                    {
                        p_curr->b_used = true;
                        p_curr->f_position = 0.0;
                    }
                    psz_sfile_uri = strchr( psz_sfile_data, ' ' );
                    while ( psz_sfile_uri && *psz_sfile_uri && psz_sfile_uri[ strlen( psz_sfile_uri ) - 1 ] == '\n' )
                        psz_sfile_uri[ strlen( psz_sfile_uri ) - 1 ] = 0;
                    p_curr->psz_uri = (char *)malloc( strlen( psz_sfile_uri + 1 ) + 1 );
                    if( p_curr->psz_uri == NULL )
                    {
                        free( p_curr );
                        p_curr = NULL;
                    }
                    else
                    {
                        strcpy( p_curr->psz_uri, psz_sfile_uri + 1 );
                        if( p_prev == NULL )
                            p_intf->p_sys->p_list = p_curr;
                        else
                            p_prev->p_next = p_curr;
                        p_prev = p_curr;
                    }
                }
            }
            free( psz_sfile_data );
        }
        fclose( p_sfile );
    }

    var_AddCallback( pl_Get( p_intf ), "item-change", ItemChange, p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    srpos_list_t *p_curr = p_intf->p_sys->p_list;
    srpos_list_t *p_next = NULL;
    playlist_t *p_playlist = pl_Get( p_this ); 
    char *psz_sfile_data = NULL;
    FILE *p_sfile = NULL;

    var_DelCallback( p_playlist, "item-change", ItemChange, p_intf );

    /* Save files position info */
    p_sfile = GetSettingsFile( false );
    if( p_sfile )
    {
        psz_sfile_data = (char *)malloc( 32 );
        if( psz_sfile_data )
        {
            while( p_curr != NULL )
            {
                sprintf( psz_sfile_data, "%f", p_curr->f_position );
                fwrite( psz_sfile_data, strlen( psz_sfile_data ), 1, p_sfile );
                fwrite( " ", 1, 1, p_sfile );
                fwrite( p_curr->psz_uri, strlen(p_curr->psz_uri), 1, p_sfile );
                fwrite( "\n", 1, 1, p_sfile );
                p_curr = p_curr->p_next;
            }
            free( psz_sfile_data );
        }
        fclose( p_sfile );
    }

    /* Free memory */
    p_curr = p_intf->p_sys->p_list;
    while( p_curr != NULL )
    {
        free( p_curr->psz_uri );
        p_next = p_curr->p_next;
        free( p_curr );
        p_curr = p_next;
    }
    free( p_intf->p_sys );
}

/*****************************************************************************
 * ItemChange: Playlist item change callback
 *****************************************************************************/
static int ItemChange( vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    VLC_UNUSED(psz_var);
    VLC_UNUSED(oldval);
    VLC_UNUSED(newval);

    double f_pos = 0.0;
    srpos_list_t *p_file_pos = NULL;
    input_item_t *p_item = NULL;
    intf_thread_t *p_intf = (intf_thread_t *)param;

    playlist_Unlock( (playlist_t *)p_this );    /* playlist_CurrentInput hangs sometimes */
    input_thread_t *p_input = playlist_CurrentInput( (playlist_t *)p_this );

    if( !p_input )
        return VLC_SUCCESS;
    if( p_input->b_dead )
    {
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }
    p_item = input_GetItem( p_input );
    if( !p_item || !p_item->psz_uri || !*p_item->psz_uri )
    {
        vlc_object_release( p_input );
        return VLC_SUCCESS;
    }

    /* Search for position info */
    p_file_pos = GetPositionInfo( p_intf->p_sys, p_item->psz_uri );

    if( p_file_pos )
    {
        /* Restore last saved position position */
        input_Control( p_input, INPUT_SET_POSITION, p_file_pos->f_position );
    }
    else if( VLC_SUCCESS == input_Control( p_input, INPUT_GET_POSITION, &f_pos) )
    {
        /* Save position info */
        SetPositionInfo( p_intf->p_sys, p_item->psz_uri, f_pos );
    }

    vlc_object_release( p_input );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * GetSettingsFile: Get module settings file
 *****************************************************************************/
static FILE* GetSettingsFile(const bool b_read_only)
{
    FILE *p_file = NULL;
    const char *psz_file_name = MODULE_STRING ".ini";
    char *psz_full_path;
    char *psz_dir;

    psz_dir = config_GetUserDir( VLC_CONFIG_DIR );
    if( !psz_dir )
        return NULL;
    psz_full_path = (char *)malloc( strlen( psz_dir ) + 2 + strlen( psz_file_name ) + 1 );
    if( !psz_full_path )
        return NULL;

    strcpy( psz_full_path, psz_dir );
    strcat( psz_full_path, "/" );
    strcat( psz_full_path, psz_file_name );

    p_file = fopen( psz_full_path, b_read_only ? "r" : "w" );
    
    free( psz_full_path );

    return p_file;
}

/*****************************************************************************
 * GetPositionInfo: Get position info for specified file uri
 *****************************************************************************/
static srpos_list_t* GetPositionInfo( intf_sys_t *p_this, const char *p_uri )
{
    srpos_list_t *p_find = p_this->p_list;
    while( p_find != NULL )
    {
        if( strcmp(p_find->psz_uri, p_uri ) == 0 )
        {
            if( !p_find->b_used )
            {
                /* Reset state */
                p_find->b_used = true;
                return p_find;
            }
            break;
        }
        p_find = p_find->p_next;
    }
    return NULL;
}

/*****************************************************************************
 * SetPositionInfo: Set position info for specified file uri
 *****************************************************************************/
static void SetPositionInfo( intf_sys_t *p_this, const char *p_uri, double f_pos )
{
    srpos_list_t *p_find = p_this->p_list;
    srpos_list_t *p_curr = NULL;
    srpos_list_t *p_prev = NULL;
    int n_max_items = 100;

    /* Search for current record */
    while( p_find != NULL )
    {
        if( strcmp( p_find->psz_uri, p_uri ) == 0 )
        {
            p_curr = p_find;
            break;
        }
        p_prev = p_find;
        p_find = p_find->p_next;
    }

    if( p_curr != NULL )
    {
        if( p_prev != NULL )
            p_prev->p_next = p_curr->p_next;
        else
            p_this->p_list = p_curr->p_next;
        free( p_curr->psz_uri );
        free( p_curr );
    }

    /* Save new record data at the top of list */
    p_curr = (srpos_list_t*)malloc( sizeof( srpos_list_t ) );
    if ( p_curr != NULL )
    {
        p_curr->psz_uri = (char *)malloc( strlen( p_uri ) + 1 );
        if( p_curr->psz_uri == NULL )
        {
            free( p_curr );
            return;
        }
        strcpy( p_curr->psz_uri, p_uri );
        p_curr->f_position = f_pos;
        p_curr->b_used = true;
        p_curr->p_next = p_this->p_list;
        p_this->p_list = p_curr;
    }

    //Remove last records and mark current items
    p_prev = p_this->p_list;
    p_curr = p_this->p_list->p_next;
    while( --n_max_items && p_curr != NULL )
    {
        p_curr->b_used = false;
        p_prev = p_curr;
        p_curr = p_curr->p_next;
    }
    if ( p_curr != NULL)
        p_prev->p_next = NULL;
    while( p_curr != NULL )
    {
        free( p_curr->psz_uri );
        p_find = p_curr->p_next;
        free( p_curr );
        p_curr = p_find;
    }
}
