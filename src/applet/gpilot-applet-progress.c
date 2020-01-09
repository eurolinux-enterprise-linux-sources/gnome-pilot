/* Generated by GOB (v2.0.15)   (do not edit directly) */

/* End world hunger, donate to the World Food Programme, http://www.wfp.org */

#define GOB_VERSION_MAJOR 2
#define GOB_VERSION_MINOR 0
#define GOB_VERSION_PATCHLEVEL 15

#define selfp (self->_priv)

#include <string.h> /* memset() */

#include "gpilot-applet-progress.h"

#include "gpilot-applet-progress-private.h"

#ifdef G_LIKELY
#define ___GOB_LIKELY(expr) G_LIKELY(expr)
#define ___GOB_UNLIKELY(expr) G_UNLIKELY(expr)
#else /* ! G_LIKELY */
#define ___GOB_LIKELY(expr) (expr)
#define ___GOB_UNLIKELY(expr) (expr)
#endif /* G_LIKELY */

#line 9 "gpilot-applet-progress.gob"


#include <gtk/gtk.h>

static gboolean timeout (GPilotAppletProgress *obj);


#line 34 "gpilot-applet-progress.c"
/* self casting macros */
#define SELF(x) GPILOT_APPLET_PROGRESS(x)
#define SELF_CONST(x) GPILOT_APPLET_PROGRESS_CONST(x)
#define IS_SELF(x) GPILOT_IS_APPLET_PROGRESS(x)
#define TYPE_SELF GPILOT_TYPE_APPLET_PROGRESS
#define SELF_CLASS(x) GPILOT_APPLET_PROGRESS_CLASS(x)

#define SELF_GET_CLASS(x) GPILOT_APPLET_PROGRESS_GET_CLASS(x)

/* self typedefs */
typedef GPilotAppletProgress Self;
typedef GPilotAppletProgressClass SelfClass;

/* here are local prototypes */
static void ___object_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void ___object_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gpilot_applet_progress_class_init (GPilotAppletProgressClass * c) G_GNUC_UNUSED;
static void gpilot_applet_progress_init (GPilotAppletProgress * self) G_GNUC_UNUSED;

enum {
	PROP_0,
	PROP_PROGRESS,
	PROP_ALIVE
};

/* pointer to the class of our parent */
static GtkObjectClass *parent_class = NULL;

/* Short form macros */
#define self_get_progress gpilot_applet_progress_get_progress
#define self_set_progress gpilot_applet_progress_set_progress
#define self_get_alive gpilot_applet_progress_get_alive
#define self_set_alive gpilot_applet_progress_set_alive
#define self_new gpilot_applet_progress_new
#define self_start gpilot_applet_progress_start
#define self_stop gpilot_applet_progress_stop
GType
gpilot_applet_progress_get_type (void)
{
	static GType type = 0;

	if ___GOB_UNLIKELY(type == 0) {
		static const GTypeInfo info = {
			sizeof (GPilotAppletProgressClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gpilot_applet_progress_class_init,
			(GClassFinalizeFunc) NULL,
			NULL /* class_data */,
			sizeof (GPilotAppletProgress),
			0 /* n_preallocs */,
			(GInstanceInitFunc) gpilot_applet_progress_init,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_OBJECT, "GPilotAppletProgress", &info, (GTypeFlags)0);
	}

	return type;
}

/* a macro for creating a new object of our type */
#define GET_NEW ((GPilotAppletProgress *)g_object_new(gpilot_applet_progress_get_type(), NULL))

/* a function for creating a new object of our type */
#include <stdarg.h>
static GPilotAppletProgress * GET_NEW_VARG (const char *first, ...) G_GNUC_UNUSED;
static GPilotAppletProgress *
GET_NEW_VARG (const char *first, ...)
{
	GPilotAppletProgress *ret;
	va_list ap;
	va_start (ap, first);
	ret = (GPilotAppletProgress *)g_object_new_valist (gpilot_applet_progress_get_type (), first, ap);
	va_end (ap);
	return ret;
}


static void
___finalize(GObject *obj_self)
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::finalize"
	GPilotAppletProgress *self G_GNUC_UNUSED = GPILOT_APPLET_PROGRESS (obj_self);
	gpointer priv G_GNUC_UNUSED = self->_priv;
	if(G_OBJECT_CLASS(parent_class)->finalize) \
		(* G_OBJECT_CLASS(parent_class)->finalize)(obj_self);
}
#undef __GOB_FUNCTION__

static void 
gpilot_applet_progress_class_init (GPilotAppletProgressClass * c G_GNUC_UNUSED)
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::class_init"
	GObjectClass *g_object_class G_GNUC_UNUSED = (GObjectClass*) c;

	g_type_class_add_private(c,sizeof(GPilotAppletProgressPrivate));

	parent_class = g_type_class_ref (GTK_TYPE_OBJECT);

	g_object_class->finalize = ___finalize;
	g_object_class->get_property = ___object_get_property;
	g_object_class->set_property = ___object_set_property;
    {
	GParamSpec   *param_spec;

	param_spec = g_param_spec_pointer ("progress", NULL, NULL,
		(GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (g_object_class,
		PROP_PROGRESS, param_spec);
	param_spec = g_param_spec_boolean ("alive", NULL, NULL,
		FALSE,
		(GParamFlags)(G_PARAM_READABLE | G_PARAM_WRITABLE));
	g_object_class_install_property (g_object_class,
		PROP_ALIVE, param_spec);
    }
}
#undef __GOB_FUNCTION__
#line 26 "gpilot-applet-progress.gob"
static void 
gpilot_applet_progress_init (GPilotAppletProgress * self G_GNUC_UNUSED)
#line 156 "gpilot-applet-progress.c"
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::init"
	self->_priv = G_TYPE_INSTANCE_GET_PRIVATE(self,GPILOT_TYPE_APPLET_PROGRESS,GPilotAppletProgressPrivate);
 {
#line 26 "gpilot-applet-progress.gob"

		self->alive = FALSE;
		self->progress = NULL;
	
#line 166 "gpilot-applet-progress.c"
 }
}
#undef __GOB_FUNCTION__

static void
___object_set_property (GObject *object,
	guint property_id,
	const GValue *VAL G_GNUC_UNUSED,
	GParamSpec *pspec G_GNUC_UNUSED)
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::set_property"
{
	GPilotAppletProgress *self G_GNUC_UNUSED;

	self = GPILOT_APPLET_PROGRESS (object);

	switch (property_id) {
	case PROP_PROGRESS:
	{	GtkProgress *  ARG G_GNUC_UNUSED = (GtkProgress * ) g_value_get_pointer (VAL);
		{
#line 19 "gpilot-applet-progress.gob"
self->progress = ARG;
#line 188 "gpilot-applet-progress.c"
		}
		break;
	}
	case PROP_ALIVE:
	{	gboolean  ARG G_GNUC_UNUSED = (gboolean ) g_value_get_boolean (VAL);
		{
#line 21 "gpilot-applet-progress.gob"
self->alive = ARG;
#line 197 "gpilot-applet-progress.c"
		}
		break;
	}
	default:
/* Apparently in g++ this is needed, glib is b0rk */
#ifndef __PRETTY_FUNCTION__
#  undef G_STRLOC
#  define G_STRLOC	__FILE__ ":" G_STRINGIFY (__LINE__)
#endif
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}
#undef __GOB_FUNCTION__

static void
___object_get_property (GObject *object,
	guint property_id,
	GValue *VAL G_GNUC_UNUSED,
	GParamSpec *pspec G_GNUC_UNUSED)
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::get_property"
{
	GPilotAppletProgress *self G_GNUC_UNUSED;

	self = GPILOT_APPLET_PROGRESS (object);

	switch (property_id) {
	case PROP_PROGRESS:
	{	GtkProgress *  ARG;
	memset (&ARG, 0, sizeof (GtkProgress * ));
		{
#line 19 "gpilot-applet-progress.gob"
ARG = self->progress;
#line 231 "gpilot-applet-progress.c"
		}
		g_value_set_pointer (VAL, ARG);
		break;
	}
	case PROP_ALIVE:
	{	gboolean  ARG;
	memset (&ARG, 0, sizeof (gboolean ));
		{
#line 21 "gpilot-applet-progress.gob"
ARG = self->alive;
#line 242 "gpilot-applet-progress.c"
		}
		g_value_set_boolean (VAL, ARG);
		break;
	}
	default:
/* Apparently in g++ this is needed, glib is b0rk */
#ifndef __PRETTY_FUNCTION__
#  undef G_STRLOC
#  define G_STRLOC	__FILE__ ":" G_STRINGIFY (__LINE__)
#endif
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}
#undef __GOB_FUNCTION__


#line 19 "gpilot-applet-progress.gob"
GtkProgress * 
gpilot_applet_progress_get_progress (GPilotAppletProgress * self)
#line 263 "gpilot-applet-progress.c"
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::get_progress"
{
#line 19 "gpilot-applet-progress.gob"
		GtkProgress* val; g_object_get (G_OBJECT (self), "progress", &val, NULL); return val;
}}
#line 270 "gpilot-applet-progress.c"
#undef __GOB_FUNCTION__

#line 19 "gpilot-applet-progress.gob"
void 
gpilot_applet_progress_set_progress (GPilotAppletProgress * self, GtkProgress * val)
#line 276 "gpilot-applet-progress.c"
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::set_progress"
{
#line 19 "gpilot-applet-progress.gob"
		g_object_set (G_OBJECT (self), "progress", val, NULL);
}}
#line 283 "gpilot-applet-progress.c"
#undef __GOB_FUNCTION__

#line 21 "gpilot-applet-progress.gob"
gboolean 
gpilot_applet_progress_get_alive (GPilotAppletProgress * self)
#line 289 "gpilot-applet-progress.c"
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::get_alive"
{
#line 21 "gpilot-applet-progress.gob"
		gboolean val; g_object_get (G_OBJECT (self), "alive", &val, NULL); return val;
}}
#line 296 "gpilot-applet-progress.c"
#undef __GOB_FUNCTION__

#line 21 "gpilot-applet-progress.gob"
void 
gpilot_applet_progress_set_alive (GPilotAppletProgress * self, gboolean val)
#line 302 "gpilot-applet-progress.c"
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::set_alive"
{
#line 21 "gpilot-applet-progress.gob"
		g_object_set (G_OBJECT (self), "alive", val, NULL);
}}
#line 309 "gpilot-applet-progress.c"
#undef __GOB_FUNCTION__


#line 31 "gpilot-applet-progress.gob"
GtkObject * 
gpilot_applet_progress_new (void)
#line 316 "gpilot-applet-progress.c"
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::new"
{
#line 32 "gpilot-applet-progress.gob"
	
		return (GtkObject*)GET_NEW;
	}}
#line 324 "gpilot-applet-progress.c"
#undef __GOB_FUNCTION__

#line 36 "gpilot-applet-progress.gob"
void 
gpilot_applet_progress_start (GPilotAppletProgress * self)
#line 330 "gpilot-applet-progress.c"
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::start"
#line 36 "gpilot-applet-progress.gob"
	g_return_if_fail (self != NULL);
#line 36 "gpilot-applet-progress.gob"
	g_return_if_fail (GPILOT_IS_APPLET_PROGRESS (self));
#line 337 "gpilot-applet-progress.c"
{
#line 37 "gpilot-applet-progress.gob"
	
		g_assert (self->progress);
		if (!self->alive) {			
			gtk_progress_set_activity_mode (self->progress, TRUE);
			self->alive = TRUE;
			self->_priv->timeout_handler = gtk_timeout_add (10, (GtkFunction) timeout, self);
		}
	}}
#line 348 "gpilot-applet-progress.c"
#undef __GOB_FUNCTION__

#line 46 "gpilot-applet-progress.gob"
void 
gpilot_applet_progress_stop (GPilotAppletProgress * self)
#line 354 "gpilot-applet-progress.c"
{
#define __GOB_FUNCTION__ "GPilot:Applet:Progress::stop"
#line 46 "gpilot-applet-progress.gob"
	g_return_if_fail (self != NULL);
#line 46 "gpilot-applet-progress.gob"
	g_return_if_fail (GPILOT_IS_APPLET_PROGRESS (self));
#line 361 "gpilot-applet-progress.c"
{
#line 47 "gpilot-applet-progress.gob"
	
		g_assert (self->progress);
		if (self->alive) {
			gtk_progress_set_activity_mode (self->progress, FALSE);
			self->alive = FALSE;
		}
	}}
#line 371 "gpilot-applet-progress.c"
#undef __GOB_FUNCTION__

#line 57 "gpilot-applet-progress.gob"


static gboolean
timeout (GPilotAppletProgress *obj) {
	static int val = 0;
	GtkProgress *progress = gpilot_applet_progress_get_progress (obj);
	gtk_progress_set_value (progress, val);
	val += 10;
	if (val > 100) {
		val = 0;
	}
/*
	g_message ("applet progress timeout: val = %d, res = %s", 
		   val, gpilot_applet_progress_get_alive (obj) ? "TRUE" : "FALSE");
*/
	return gpilot_applet_progress_get_alive (obj);
}


#line 394 "gpilot-applet-progress.c"
