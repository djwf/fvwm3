/* Copyright (C) 2001  Olivier Chapuis
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 */

#include "config.h"

#ifdef HAVE_EWMH

#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xmd.h>

#include "libs/fvwmlib.h"
#include "fvwm.h"
#include "cursor.h"
#include "functions.h"
#include "misc.h"
#include "screen.h"
#include "virtual.h"
#include "commands.h"
#include "update.h"
#include "style.h"
#include "focus.h"
#include "stack.h"
#include "events.h"
#include "externs.h"
#include "ewmh.h"
#include "ewmh_intern.h"

extern ewmh_atom ewmh_atom_wm_state[];

#define DEBUG_EWMH_INIT_STATE 0
/* ************************************************************************* *
 * root
 * ************************************************************************* */
int ewmh_CurrentDesktop(EWMH_CMD_ARGS)
{
  goto_desk(ev->xclient.data.l[0]);
  return -1;
}

int ewmh_DesktopGeometry(EWMH_CMD_ARGS)
{
  char action[256];
  sprintf(action, "%ld %ld", ev->xclient.data.l[0], ev->xclient.data.l[1]);
  CMD_DesktopSize(NULL, NULL, None, NULL, C_ROOT, action, NULL);
  return -1;
}

int ewmh_DesktopViewPort(EWMH_CMD_ARGS)
{
  MoveViewport(ev->xclient.data.l[0], ev->xclient.data.l[1], 1);
  return -1;
}

int ewmh_NumberOfDesktops(EWMH_CMD_ARGS)
{
  int d = ev->xclient.data.l[0];

  /* not a lot of sinification for FVWM */
  if (d > 0 && (d <= ewmhc.MaxDesktops || ewmhc.MaxDesktops == 0))
  {
    ewmhc.NumberOfDesktops = d;
    EWMH_SetNumberOfDesktops();
  }
  return -1;
}

/* ************************************************************************* *
 * window
 * ************************************************************************* */

int ewmh_ActiveWindow(EWMH_CMD_ARGS)
{
  if (ev == NULL)
    return 0;

  old_execute_function(
    NULL, "EWMHActivateWindowFunc", fwin, ev, C_WINDOW, -1,0,NULL);
  return 0;
}

int ewmh_CloseWindow(EWMH_CMD_ARGS)
{
  if (ev == NULL)
    return 0;

  CMD_Close(NULL, ev, FW_W(fwin), fwin, C_WINDOW, "", NULL);
  return 0;
}

int ewmh_WMDesktop(EWMH_CMD_ARGS)
{
  if (ev != NULL && style == NULL)
  {
    /* client message */
    unsigned long d = (unsigned long)ev->xclient.data.l[0];

    /* the spec says that if d = 0xFFFFFFFF then we have to Stick the window
    *  however KDE use 0xFFFFFFFE :o) */
    if (d == 0xFFFFFFFE || d == 0xFFFFFFFF)
    {
      CMD_Stick(NULL, ev, FW_W(fwin), fwin, C_WINDOW, "On", NULL);
    }
    else if (d >= 0)
    {
      if (IS_STICKY(fwin))
	 CMD_Stick(NULL, ev, FW_W(fwin), fwin, C_WINDOW, "Off", NULL);
      if (fwin->Desk != d)
	do_move_window_to_desk(fwin, (int)d);
    }
    return 0;
  }

  if (style != NULL && ev == NULL)
  {
    /* start on desk */
    unsigned long *val;
    unsigned int size = 0;

    if (HAS_EWMH_INIT_WM_DESKTOP(fwin) != EWMH_STATE_UNDEFINED_HINT)
      return 0;

    val = ewmh_AtomGetByName(FW_W(fwin),
			     "_NET_WM_DESKTOP",
			     EWMH_ATOM_LIST_CLIENT_WIN,
			     &size);

    if (val == NULL)
    {
      SET_HAS_EWMH_INIT_WM_DESKTOP(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }
#if DEBUG_EWMH_INIT_STATE
    fprintf(stderr,"ewmh WM_DESKTOP hint for window 0x%lx  (%i,%lu,%lu)\n",
	    FW_W(fwin), HAS_EWMH_INIT_WM_DESKTOP(fwin),
	    fwin->ewmh_hint_desktop, val[0]);
#endif
    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      if ((val[0] == 0xFFFFFFFE || val[0] == 0xFFFFFFFF))
      {
	SFSET_IS_STICKY(*style, 1);
	SMSET_IS_STICKY(*style, 1);
	SCSET_IS_STICKY(*style, 1);
      }
      else if (val[0] < 256)
      {
	/* prevent crazy hints ?? */
	style->flags.use_start_on_desk = 1;
	style->flag_mask.use_start_on_desk = 1;
	style->change_mask.use_start_on_desk = 1;
	SSET_START_DESK(*style, val[0]);
      }
    }
    SET_HAS_EWMH_INIT_WM_DESKTOP(fwin, EWMH_STATE_HAS_HINT);
    fwin->ewmh_hint_desktop = val[0];
    free(val);
  }
  return 0;
}

int ewmh_MoveResize(EWMH_CMD_ARGS)
{
  if (ev == NULL)
    return 0;

  {
    int dir = ev->xclient.data.l[2];
    int x_warp = 0;
    int y_warp = 0;
    Bool move = False;
    char cmd[256];

    switch(dir)
    {
      case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
	break;
      case _NET_WM_MOVERESIZE_SIZE_TOP:
	x_warp = 50;
	break;
      case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
	x_warp = 100;
	break;
      case _NET_WM_MOVERESIZE_SIZE_RIGHT:
	x_warp = 100; y_warp = 50;
	break;
      case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
	x_warp = 100; y_warp = 100;
	break;
      case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
	x_warp = 50; y_warp = 100;
	break;
      case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
	y_warp = 100;
	break;
      case _NET_WM_MOVERESIZE_SIZE_LEFT:
	y_warp = 50;
	break;
      case _NET_WM_MOVERESIZE_MOVE:
	x_warp = 50; y_warp = 50; /* why not */
	move = True;
	break;
    }
    sprintf(cmd,"%i %i",x_warp,y_warp);
    CMD_WarpToWindow(NULL, ev, FW_W(fwin), fwin, C_WINDOW, cmd, NULL);
    if (move)
      CMD_Move(NULL, ev, FW_W(fwin), fwin, C_WINDOW, "", NULL);
    else
      CMD_Resize(NULL, ev, FW_W(fwin), fwin, C_WINDOW, "", NULL);

    return 0;
  }
}

/* ************************************************************************* *
 * WM_STATE*
 * ************************************************************************* */
int ewmh_WMState(EWMH_CMD_ARGS)
{
  unsigned long maximize = 0;

  if (ev != NULL)
  {
    ewmh_atom *a1,*a2;

    a1 = ewmh_GetEwmhAtomByAtom(ev->xclient.data.l[1], EWMH_ATOM_LIST_WM_STATE);
    a2 = ewmh_GetEwmhAtomByAtom(ev->xclient.data.l[2], EWMH_ATOM_LIST_WM_STATE);

    if (a1 != NULL)
    {
      maximize |= a1->action(fwin, ev, NULL, 0);
    }
    if (a2 != NULL)
    {
      maximize |= a2->action(fwin, ev, NULL, 0);
    }
  }
  else if (style != NULL)
  {
    Atom *val;
    unsigned int size = 0;
    int i;
    ewmh_atom *list = ewmh_atom_wm_state;
    int has_hint = 0;

    val = ewmh_AtomGetByName(FW_W(fwin),
			     "_NET_WM_STATE",
			     EWMH_ATOM_LIST_CLIENT_WIN,
			     &size);

    if (val == NULL)
      size = 0;

#if DEBUG_EWMH_INIT_STATE
    if (size != 0)
      fprintf(stderr,"Window 0x%lx has an init _NET_WM_STATE hint\n",FW_W(fwin));
#endif
    while(list->name != NULL)
    {
      has_hint = 0;
      for(i = 0; i < (size / (sizeof(Atom))); i++)
      {
	if (list->atom == val[i])
	  has_hint = 1;
      }
      maximize |= list->action(fwin, NULL, style, has_hint);
      list++;
    }
    if (val != NULL)
      free(val);
    return 0;
  }

  if (maximize != 0)
  {
    int max_vert = (maximize & EWMH_MAXIMIZE_VERT)? 100:0;
    int max_horiz = (maximize & EWMH_MAXIMIZE_HORIZ)? 100:0;
    char cmd[256];

    if (maximize & EWMH_MAXIMIZE_REMOVE)
      sprintf(cmd,"%s", "Off");
    else
      sprintf(cmd,"%s %i %i", "On", max_horiz, max_vert);
    CMD_Maximize(NULL, ev, FW_W(fwin), fwin, C_WINDOW, cmd, NULL);
  }
  return 0;
}

/* not yet implemented */
int ewmh_WMStateFullScreen(EWMH_CMD_ARGS)
{

  if (ev == NULL && style == NULL)
  {
    return 0;
  }

  if (ev == NULL && style != NULL)
  {
    /* start full screen */
    int ret = 0;
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    if (has_hint)
      fprintf(stderr,"\tFullscreen\n");
#endif
    if (HAS_EWMH_INIT_FULLSCREEN_STATE(fwin) != EWMH_STATE_UNDEFINED_HINT)
      return 0;
    if (!has_hint)
    {
      SET_HAS_EWMH_INIT_FULLSCREEN_STATE(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }
    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      ret = EWMH_MAXIMIZE_FULLSCREEN;
    }
    SET_HAS_EWMH_INIT_FULLSCREEN_STATE(fwin, EWMH_STATE_HAS_HINT);
    return 0;
  }

  return 0;
}

int ewmh_WMStateHidden(EWMH_CMD_ARGS)
{
  if (ev == NULL && style == NULL)
  {
    unsigned long do_restore = any;

    if (do_restore)
    {
      if (HAS_EWMH_INIT_HIDDEN_STATE(fwin) == EWMH_STATE_HAS_HINT)
	return True;
      return False;
    }
    return IS_ICONIFIED(fwin);
  }

  if (ev == NULL && style != NULL)
  {
    /* start iconified */
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    if (has_hint)
      fprintf(stderr,"\tHidden\n");
#endif
    if (HAS_EWMH_INIT_HIDDEN_STATE(fwin) != EWMH_STATE_UNDEFINED_HINT)
      return 0;
    if (!has_hint)
    {
      SET_HAS_EWMH_INIT_HIDDEN_STATE(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }
    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      style->flags.do_start_iconic = 1;
      style->flag_mask.do_start_iconic = 1;
      style->change_mask.do_start_iconic = 1;
    }
    SET_HAS_EWMH_INIT_HIDDEN_STATE(fwin, EWMH_STATE_HAS_HINT);
    return 0;
  }

  if (ev != NULL)
  {
    /* client message */
    char cmd[8];
    int bool_arg = ev->xclient.data.l[0];

    if ((bool_arg == NET_WM_STATE_TOGGLE && !IS_ICONIFIED(fwin)) ||
	bool_arg == NET_WM_STATE_ADD)
    {
      /* iconify */
      sprintf(cmd,"%s", "On");
    }
    else
    {
      /* deiconify */
      sprintf(cmd,"%s", "Off");
    }
    CMD_Iconify(NULL, ev, FW_W(fwin), fwin, C_WINDOW, cmd, NULL);
  }
  return 0;
}

int ewmh_WMStateMaxHoriz(EWMH_CMD_ARGS)
{

  if (ev == NULL && style == NULL)
  {
    return IS_MAXIMIZED(fwin);
  }

  if (ev == NULL && style != NULL)
  {
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    if (has_hint)
      fprintf(stderr,"\t Maxhoriz %i\n", HAS_EWMH_INIT_MAXHORIZ_STATE(fwin));
#endif
    if (HAS_EWMH_INIT_MAXHORIZ_STATE(fwin) != EWMH_STATE_UNDEFINED_HINT)
      return 0;
    if (!has_hint)
    {
      SET_HAS_EWMH_INIT_MAXHORIZ_STATE(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }
    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      /* SET_MAXIMIZED(fwin, 1); */
    }
    SET_HAS_EWMH_INIT_MAXHORIZ_STATE(fwin, EWMH_STATE_HAS_HINT);
    return 0;
  }

  {
    /* client message */
    int bool_arg = ev->xclient.data.l[0];
    if ((bool_arg == NET_WM_STATE_TOGGLE && !IS_MAXIMIZED(fwin)) ||
	bool_arg == NET_WM_STATE_ADD)
      return EWMH_MAXIMIZE_HORIZ;
    else
      return EWMH_MAXIMIZE_REMOVE;
  }
}

int ewmh_WMStateMaxVert(EWMH_CMD_ARGS)
{

  if (ev == NULL && style == NULL)
  {
    return IS_MAXIMIZED(fwin);
  }

  if (ev == NULL && style != NULL)
  {
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    if (has_hint)
      fprintf(stderr,"\t Maxvert %i\n", HAS_EWMH_INIT_MAXVERT_STATE(fwin));
#endif
    if (HAS_EWMH_INIT_MAXVERT_STATE(fwin) != EWMH_STATE_UNDEFINED_HINT)
      return 0;
    if (!has_hint)
    {
      SET_HAS_EWMH_INIT_MAXVERT_STATE(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }
    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      /* SET_MAXIMIZED(fwin, 1); */
    }
    SET_HAS_EWMH_INIT_MAXVERT_STATE(fwin, EWMH_STATE_HAS_HINT);
    return 0;
  }

  {
    /* client message */
    int bool_arg = ev->xclient.data.l[0];
    if ((bool_arg == NET_WM_STATE_TOGGLE && !IS_MAXIMIZED(fwin)) ||
	bool_arg == NET_WM_STATE_ADD)
      return EWMH_MAXIMIZE_VERT;
    else
      return EWMH_MAXIMIZE_REMOVE;
  }
}

int ewmh_WMStateModal(EWMH_CMD_ARGS)
{
  if (ev == NULL && style == NULL)
  {
    unsigned long do_restore = any;

    if (do_restore)
    {
      if (HAS_EWMH_INIT_MODAL_STATE(fwin) == EWMH_STATE_HAS_HINT)
	return True;
      return False;
    }
    return IS_EWMH_MODAL(fwin);
  }

  if (ev == NULL && style != NULL)
  {
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    if (has_hint)
      fprintf(stderr,"\t Modal %i\n", HAS_EWMH_INIT_MODAL_STATE(fwin));
#endif
    if (!has_hint &&
	HAS_EWMH_INIT_MODAL_STATE(fwin) != EWMH_STATE_HAS_HINT)
    {
      SET_HAS_EWMH_INIT_MODAL_STATE(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }

    /* the window map or had mapped with a modal hint */
    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      SET_EWMH_MODAL(fwin, True);
      /* the window is a modal transient window so we grab the focus
       * it will be good to raise it but ... */
      SFSET_DO_GRAB_FOCUS_WHEN_TRANSIENT_CREATED(*style, 1);
      SMSET_DO_GRAB_FOCUS_WHEN_TRANSIENT_CREATED(*style, 1);
      SCSET_DO_GRAB_FOCUS_WHEN_TRANSIENT_CREATED(*style, 1);
    }
    else
    {
      SET_EWMH_MODAL(fwin, False);
      if (!DO_GRAB_FOCUS_TRANSIENT(style))
      {
	SFSET_DO_GRAB_FOCUS_WHEN_TRANSIENT_CREATED(*style, 0);
	SMSET_DO_GRAB_FOCUS_WHEN_TRANSIENT_CREATED(*style, 1);
	SCSET_DO_GRAB_FOCUS_WHEN_TRANSIENT_CREATED(*style, 1);
      }
    }
    SET_HAS_EWMH_INIT_MODAL_STATE(fwin, EWMH_STATE_HAS_HINT);
    return 0;
  }

  if (ev != NULL && fwin != NULL)
  {
    /* client message: I do not think we can get such message */
    int bool_arg = ev->xclient.data.l[0];
    if ((bool_arg == NET_WM_STATE_TOGGLE && !IS_EWMH_MODAL(fwin)) ||
	bool_arg == NET_WM_STATE_ADD)
    {
    }
    else
    {
    }
  }
  return 0;
}

int ewmh_WMStateShaded(EWMH_CMD_ARGS)
{

  if (ev == NULL && style == NULL)
  {
    unsigned long do_restore = any;

    if (do_restore)
    {
      if (HAS_EWMH_INIT_SHADED_STATE(fwin) == EWMH_STATE_HAS_HINT)
	return True;
      return False;
    }
    return IS_SHADED(fwin);
  }

  if (ev == NULL && style != NULL)
  {
    /* start shaded */
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    if (has_hint)
      fprintf(stderr,"\t Shaded %i\n", HAS_EWMH_INIT_SHADED_STATE(fwin));
#endif
    if (HAS_EWMH_INIT_SHADED_STATE(fwin) != EWMH_STATE_UNDEFINED_HINT)
      return 0;
    if (!has_hint)
    {
      SET_HAS_EWMH_INIT_SHADED_STATE(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }
    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      SET_SHADED(fwin, 1);
      SET_SHADED_DIR(fwin, GET_TITLE_DIR(fwin));
    }
    SET_HAS_EWMH_INIT_SHADED_STATE(fwin, EWMH_STATE_HAS_HINT);
    return 0;
  }

  {
    /* client message */
    int bool_arg = ev->xclient.data.l[0];
    if ((bool_arg == NET_WM_STATE_TOGGLE && !IS_SHADED(fwin)) ||
	bool_arg == NET_WM_STATE_ADD)
    {
      CMD_WindowShade(NULL, ev, FW_W(fwin), fwin, C_WINDOW, "True", NULL);
    }
    else
    {
      CMD_WindowShade(NULL, ev, FW_W(fwin), fwin, C_WINDOW, "False", NULL);
    }
  }
  return 0;
}

int ewmh_WMStateSkipPager(EWMH_CMD_ARGS)
{
  if (ev == NULL && style == NULL)
  {
    unsigned long do_restore = any;

    if (do_restore)
    {
      if (HAS_EWMH_INIT_SKIP_PAGER_STATE(fwin) == EWMH_STATE_HAS_HINT)
	return True;
      return False;
    }
    return DO_SKIP_WINDOW_LIST(fwin);
  }

  if (ev == NULL && style != NULL)
  {
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    /*if (has_hint)*/
      fprintf(stderr,"\t Skip_Pager %lu, %i, %i\n",
	      has_hint,
	      HAS_EWMH_INIT_SKIP_PAGER_STATE(fwin),
	      DO_EWMH_IGNORE_STATE_HINTS(style));
#endif
    if (!has_hint &&
	HAS_EWMH_INIT_SKIP_PAGER_STATE(fwin) != EWMH_STATE_HAS_HINT)
    {
      SET_HAS_EWMH_INIT_SKIP_PAGER_STATE(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }
    if (has_hint && HAS_EWMH_INIT_SKIP_PAGER_STATE(fwin) == EWMH_STATE_NO_HINT)
    {
      return 0;
    }

    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      SFSET_DO_WINDOW_LIST_SKIP(*style, 1);
      SMSET_DO_WINDOW_LIST_SKIP(*style, 1);
      SCSET_DO_WINDOW_LIST_SKIP(*style, 1);
    }
    else if (!DO_SKIP_WINDOW_LIST(style))
    {
      SFSET_DO_WINDOW_LIST_SKIP(*style, 0);
      SMSET_DO_WINDOW_LIST_SKIP(*style, 1);
      SCSET_DO_WINDOW_LIST_SKIP(*style, 1);
    }
    SET_HAS_EWMH_INIT_SKIP_PAGER_STATE(fwin, EWMH_STATE_HAS_HINT);
    return 0;
  }

  {
    /* I do not think we can get such client message */
    int bool_arg = ev->xclient.data.l[0];

    if ((bool_arg == NET_WM_STATE_TOGGLE && !DO_SKIP_WINDOW_LIST(fwin)) ||
	bool_arg == NET_WM_STATE_ADD)
    {
    }
    else
    {
    }
  }
  return 0;
}

int ewmh_WMStateSkipTaskBar(EWMH_CMD_ARGS)
{
  if (ev == NULL && style == NULL)
  {
    unsigned long do_restore = any;

    if (do_restore)
    {
      if (HAS_EWMH_INIT_SKIP_TASKBAR_STATE(fwin) == EWMH_STATE_HAS_HINT)
	return True;
      return False;
    }
    return DO_SKIP_WINDOW_LIST(fwin);
  }

  if (ev == NULL && style != NULL)
  {
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    /*if (has_hint)*/
      fprintf(stderr,"\t Skip_Taskbar %lu, %i, %i\n",
	      has_hint,
	      HAS_EWMH_INIT_SKIP_TASKBAR_STATE(fwin),
	      DO_EWMH_IGNORE_STATE_HINTS(style));
#endif
    if (!has_hint &&
	HAS_EWMH_INIT_SKIP_TASKBAR_STATE(fwin) != EWMH_STATE_HAS_HINT)
    {
      SET_HAS_EWMH_INIT_SKIP_TASKBAR_STATE(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }
    if (has_hint && HAS_EWMH_INIT_SKIP_TASKBAR_STATE(fwin) == EWMH_STATE_NO_HINT)
    {
      return 0;
    }

    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      SFSET_DO_WINDOW_LIST_SKIP(*style, 1);
      SMSET_DO_WINDOW_LIST_SKIP(*style, 1);
      SCSET_DO_WINDOW_LIST_SKIP(*style, 1);
    }
    else if (!DO_SKIP_WINDOW_LIST(style))
    {
      SFSET_DO_WINDOW_LIST_SKIP(*style, 0);
      SMSET_DO_WINDOW_LIST_SKIP(*style, 1);
      SCSET_DO_WINDOW_LIST_SKIP(*style, 1);
    }
    SET_HAS_EWMH_INIT_SKIP_TASKBAR_STATE(fwin, EWMH_STATE_HAS_HINT);
    return 0;
  }

  {
    /* I do not think we can get such client message */
    int bool_arg = ev->xclient.data.l[0];

    if ((bool_arg == NET_WM_STATE_TOGGLE && !DO_SKIP_WINDOW_LIST(fwin)) ||
	bool_arg == NET_WM_STATE_ADD)
    {
    }
    else
    {
    }
  }
  return 0;
}

int ewmh_WMStateStaysOnTop(EWMH_CMD_ARGS)
{
  if (ev == NULL && style == NULL)
  {
    unsigned long do_restore = any;

    if (do_restore)
    {
      if (fwin->ewmh_hint_layer == Scr.TopLayer)
	return True;
      return False;
    }
    if (fwin->layer >= Scr.TopLayer)
      return True;
    return False;
  }

  if (ev == NULL && style != NULL)
  {
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    if (has_hint)
      fprintf(stderr,"\tStaysOnTop\n");
#endif
    if (!has_hint && fwin->ewmh_hint_layer == 0)
    {
      return 0;
    }

    fwin->ewmh_hint_layer = Scr.TopLayer;
    if (DO_EWMH_USE_STACKING_HINTS(style))
    {
      SSET_LAYER(*style, Scr.TopLayer);
      style->flags.use_layer = 1;
      style->flag_mask.use_layer = 1;
      style->change_mask.use_layer = 1;
    }
    else if (!style->change_mask.use_layer)
    {
      SSET_LAYER(*style, Scr.DefaultLayer);
      style->flags.use_layer = 1;
      style->flag_mask.use_layer = 1;
      style->change_mask.use_layer = 1;
    }
    return 0;
  }

  if (ev != NULL)
  {
    /* client message */
    int bool_arg = ev->xclient.data.l[0];

    if ((bool_arg == NET_WM_STATE_TOGGLE && fwin->layer < Scr.TopLayer) ||
	bool_arg == NET_WM_STATE_ADD)
    {
      new_layer(fwin, Scr.TopLayer);
    }
    else
    {
      new_layer(fwin, Scr.DefaultLayer);
    }
  }
  return 0;
}

int ewmh_WMStateSticky(EWMH_CMD_ARGS)
{

  if (ev == NULL && style == NULL)
  {
    unsigned long do_restore = any;

    if (do_restore)
    {
      if (HAS_EWMH_INIT_STICKY_STATE(fwin) == EWMH_STATE_HAS_HINT)
	return True;
      return False;
    }
    return IS_STICKY(fwin);
  }

  if (ev == NULL && style != NULL)
  {
    /* start sticky */
    unsigned long has_hint = any;
#if DEBUG_EWMH_INIT_STATE
    if (has_hint)
      fprintf(stderr,"\t Sticky\n");
#endif
    if (HAS_EWMH_INIT_STICKY_STATE(fwin) != EWMH_STATE_UNDEFINED_HINT)
      return 0;
    if (!has_hint)
    {
      SET_HAS_EWMH_INIT_STICKY_STATE(fwin, EWMH_STATE_NO_HINT);
      return 0;
    }
    if (!DO_EWMH_IGNORE_STATE_HINTS(style))
    {
      SFSET_IS_STICKY(*style, 1);
      SMSET_IS_STICKY(*style, 1);
      SCSET_IS_STICKY(*style, 1);
    }
    SET_HAS_EWMH_INIT_STICKY_STATE(fwin, EWMH_STATE_HAS_HINT);
    return 0;
  }

  {
    /* client message */
    int bool_arg = ev->xclient.data.l[0];
    if ((bool_arg == NET_WM_STATE_TOGGLE && !IS_STICKY(fwin)) ||
	bool_arg == NET_WM_STATE_ADD)
    {
      CMD_Stick(NULL, ev, FW_W(fwin), fwin, C_WINDOW, "On", NULL);
    }
    else
    {
      CMD_Stick(NULL, ev, FW_W(fwin), fwin, C_WINDOW, "Off", NULL);
    }
  }
  return 0;
}

/* ************************************************************************* *
 * Property Notify (_NET_WM_ICON is in ewmh_icon.c, _NET_WM_*NAME are in
 * ewmh_name)			     *
 * ************************************************************************* */
int ewmh_WMIconGeometry(EWMH_CMD_ARGS)
{
  unsigned int size;
  CARD32 *val;

  /* FIXME: After a (un)silde of kicker the geometry are wrong (not because
   * we set the geometry just after the property notify). This does
   * not happen with kwin */
  val = ewmh_AtomGetByName(FW_W(fwin), "_NET_WM_ICON_GEOMETRY",
			   EWMH_ATOM_LIST_PROPERTY_NOTIFY, &size);

  if (val == NULL)
  {
    fwin->ewmh_icon_geometry.x = 0;
    fwin->ewmh_icon_geometry.y = 0;
    fwin->ewmh_icon_geometry.width = 0;
    fwin->ewmh_icon_geometry.height = 0;
    return 0;
  }

  fwin->ewmh_icon_geometry.x = val[0];
  fwin->ewmh_icon_geometry.y = val[1];
  fwin->ewmh_icon_geometry.width = val[2];
  fwin->ewmh_icon_geometry.height = val[3];

  free(val);
  return 0;
}

/**** for animation ****/
void EWMH_GetIconGeometry(FvwmWindow *fwin, rectangle *icon_rect)
{
  if (!IS_ICON_SUPPRESSED(fwin) ||
      (fwin->ewmh_icon_geometry.x == 0 &&
       fwin->ewmh_icon_geometry.y == 0 &&
       fwin->ewmh_icon_geometry.width == 0 &&
       fwin->ewmh_icon_geometry.height == 0))
    return;
  icon_rect->x = fwin->ewmh_icon_geometry.x;
  icon_rect->y = fwin->ewmh_icon_geometry.y;
  icon_rect->width = fwin->ewmh_icon_geometry.width;
  icon_rect->height = fwin->ewmh_icon_geometry.height;
  return;
}

int ewmh_WMStrut(EWMH_CMD_ARGS)
{
  unsigned int size = 0;
  CARD32 *val;

  if (ev == NULL)
  {
    fwin->dyn_strut.left = fwin->strut.left = 0;
    fwin->dyn_strut.right = fwin->strut.right = 0;
    fwin->dyn_strut.top = fwin->strut.top = 0;
    fwin->dyn_strut.bottom = fwin->strut.bottom = 0;
  }

  val = ewmh_AtomGetByName(FW_W(fwin), "_NET_WM_STRUT",
			   EWMH_ATOM_LIST_PROPERTY_NOTIFY, &size);

  if (val == NULL)
    return 0;

  if ((val[0] > 0 || val[1] > 0 || val[2] > 0 || val[3] > 0)
      &&
      (val[0] !=  fwin->strut.left || val[1] != fwin->strut.right ||
       val[2] != fwin->strut.top   || val[3] !=	 fwin->strut.bottom))
  {
    fwin->strut.left   = val[0];
    fwin->strut.right  = val[1];
    fwin->strut.top    = val[2];
    fwin->strut.bottom = val[3];
    ewmh_ComputeAndSetWorkArea();
  }
 if (val[0] !=	fwin->dyn_strut.left || val[1] != fwin->dyn_strut.right ||
     val[2] != fwin->dyn_strut.top   || val[3] !=  fwin->dyn_strut.bottom)
 {
   fwin->dyn_strut.left	  = val[0];
   fwin->dyn_strut.right  = val[1];
   fwin->dyn_strut.top	  = val[2];
   fwin->dyn_strut.bottom = val[3];
   ewmh_HandleDynamicWorkArea();
 }
 free(val);

  return 0;
}

/* ************************************************************************* *
 *
 * ************************************************************************* */
Bool EWMH_ProcessClientMessage(FvwmWindow *fwin, XEvent *ev)
{
  ewmh_atom *ewmh_a = NULL;

  if ((ewmh_a =
       (ewmh_atom *)ewmh_GetEwmhAtomByAtom(ev->xclient.message_type,
					   EWMH_ATOM_LIST_CLIENT_ROOT)) != NULL)
  {
    if (ewmh_a->action != None)
      ewmh_a->action(fwin, ev, NULL, 0);
    return True;
  }

  if (fwin == NULL || ev->xclient.window == None)
    return False;


  if ((ewmh_a =
       (ewmh_atom *)ewmh_GetEwmhAtomByAtom(ev->xclient.message_type,
					   EWMH_ATOM_LIST_CLIENT_WIN)) != NULL)
  {
    if (ewmh_a->action != None)
      ewmh_a->action(fwin, ev, NULL, 0);
    return True;
  }

  return False;
}

/* ************************************************************************* *
 *
 * ************************************************************************* */
void EWMH_ProcessPropertyNotify(FvwmWindow *fwin, XEvent *ev)
{
  ewmh_atom *ewmh_a = NULL;

  if ((ewmh_a =
       (ewmh_atom *)ewmh_GetEwmhAtomByAtom(ev->xproperty.atom,
				EWMH_ATOM_LIST_PROPERTY_NOTIFY)) != NULL)
  {
    if (ewmh_a->action != None)
    {
      flush_property_notify(ewmh_a->atom, FW_W(fwin));
      ewmh_a->action(fwin, ev, NULL, 0);
    }
  }

}

#endif /* HAVE_EWMH */
