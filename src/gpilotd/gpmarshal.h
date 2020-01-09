
#ifndef __gp_marshal_MARSHAL_H__
#define __gp_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* VOID:INT,POINTER (./gpmarshal.list:1) */
extern void gp_marshal_VOID__INT_POINTER (GClosure     *closure,
                                          GValue       *return_value,
                                          guint         n_param_values,
                                          const GValue *param_values,
                                          gpointer      invocation_hint,
                                          gpointer      marshal_data);

/* INT:POINTER,INT (./gpmarshal.list:2) */
extern void gp_marshal_INT__POINTER_INT (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* INT:POINTER,POINTER (./gpmarshal.list:3) */
extern void gp_marshal_INT__POINTER_POINTER (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* INT:POINTER,BOOL (./gpmarshal.list:4) */
extern void gp_marshal_INT__POINTER_BOOLEAN (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);
#define gp_marshal_INT__POINTER_BOOL	gp_marshal_INT__POINTER_BOOLEAN

/* INT:POINTER,INT,INT (./gpmarshal.list:5) */
extern void gp_marshal_INT__POINTER_INT_INT (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* INT:POINTER (./gpmarshal.list:6) */
extern void gp_marshal_INT__POINTER (GClosure     *closure,
                                     GValue       *return_value,
                                     guint         n_param_values,
                                     const GValue *param_values,
                                     gpointer      invocation_hint,
                                     gpointer      marshal_data);

/* INT:NONE (./gpmarshal.list:7) */
extern void gp_marshal_INT__VOID (GClosure     *closure,
                                  GValue       *return_value,
                                  guint         n_param_values,
                                  const GValue *param_values,
                                  gpointer      invocation_hint,
                                  gpointer      marshal_data);
#define gp_marshal_INT__NONE	gp_marshal_INT__VOID

G_END_DECLS

#endif /* __gp_marshal_MARSHAL_H__ */

