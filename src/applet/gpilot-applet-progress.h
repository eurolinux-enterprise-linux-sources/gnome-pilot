/* Generated by GOB (v2.0.15)   (do not edit directly) */

#include <glib.h>
#include <glib-object.h>
#ifndef __GPILOT_APPLET_PROGRESS_H__
#define __GPILOT_APPLET_PROGRESS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */



#include <string.h>
#include <config.h>
#include <gtk/gtk.h>


/*
 * Type checking and casting macros
 */
#define GPILOT_TYPE_APPLET_PROGRESS	(gpilot_applet_progress_get_type())
#define GPILOT_APPLET_PROGRESS(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gpilot_applet_progress_get_type(), GPilotAppletProgress)
#define GPILOT_APPLET_PROGRESS_CONST(obj)	G_TYPE_CHECK_INSTANCE_CAST((obj), gpilot_applet_progress_get_type(), GPilotAppletProgress const)
#define GPILOT_APPLET_PROGRESS_CLASS(klass)	G_TYPE_CHECK_CLASS_CAST((klass), gpilot_applet_progress_get_type(), GPilotAppletProgressClass)
#define GPILOT_IS_APPLET_PROGRESS(obj)	G_TYPE_CHECK_INSTANCE_TYPE((obj), gpilot_applet_progress_get_type ())

#define GPILOT_APPLET_PROGRESS_GET_CLASS(obj)	G_TYPE_INSTANCE_GET_CLASS((obj), gpilot_applet_progress_get_type(), GPilotAppletProgressClass)

/* Private structure type */
typedef struct _GPilotAppletProgressPrivate GPilotAppletProgressPrivate;

/*
 * Main object structure
 */
#ifndef __TYPEDEF_GPILOT_APPLET_PROGRESS__
#define __TYPEDEF_GPILOT_APPLET_PROGRESS__
typedef struct _GPilotAppletProgress GPilotAppletProgress;
#endif
struct _GPilotAppletProgress {
	GtkObject __parent__;
	/*< public >*/
	GtkProgress * progress;
	gboolean alive;
	/*< private >*/
	GPilotAppletProgressPrivate *_priv;
};

/*
 * Class definition
 */
typedef struct _GPilotAppletProgressClass GPilotAppletProgressClass;
struct _GPilotAppletProgressClass {
	GtkObjectClass __parent__;
};


/*
 * Public methods
 */
GType	gpilot_applet_progress_get_type	(void);
GtkProgress * 	gpilot_applet_progress_get_progress	(GPilotAppletProgress * self);
void 	gpilot_applet_progress_set_progress	(GPilotAppletProgress * self,
					GtkProgress * val);
gboolean 	gpilot_applet_progress_get_alive	(GPilotAppletProgress * self);
void 	gpilot_applet_progress_set_alive	(GPilotAppletProgress * self,
					gboolean val);
GtkObject * 	gpilot_applet_progress_new	(void);
void 	gpilot_applet_progress_start	(GPilotAppletProgress * self);
void 	gpilot_applet_progress_stop	(GPilotAppletProgress * self);

/*
 * Argument wrapping macros
 */
#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#define GPILOT_APPLET_PROGRESS_PROP_PROGRESS(arg)    	"progress", __extension__ ({GtkProgress * z = (arg); z;})
#define GPILOT_APPLET_PROGRESS_GET_PROP_PROGRESS(arg)	"progress", __extension__ ({GtkProgress * *z = (arg); z;})
#define GPILOT_APPLET_PROGRESS_PROP_ALIVE(arg)    	"alive", __extension__ ({gboolean z = (arg); z;})
#define GPILOT_APPLET_PROGRESS_GET_PROP_ALIVE(arg)	"alive", __extension__ ({gboolean *z = (arg); z;})
#else /* __GNUC__ && !__STRICT_ANSI__ */
#define GPILOT_APPLET_PROGRESS_PROP_PROGRESS(arg)    	"progress",(GtkProgress * )(arg)
#define GPILOT_APPLET_PROGRESS_GET_PROP_PROGRESS(arg)	"progress",(GtkProgress * *)(arg)
#define GPILOT_APPLET_PROGRESS_PROP_ALIVE(arg)    	"alive",(gboolean )(arg)
#define GPILOT_APPLET_PROGRESS_GET_PROP_ALIVE(arg)	"alive",(gboolean *)(arg)
#endif /* __GNUC__ && !__STRICT_ANSI__ */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
