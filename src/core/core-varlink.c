/* SPDX-License-Identifier: LGPL-2.1+ */

#include "core-varlink.h"
#include "mkdir.h"
#include "user-util.h"
#include "varlink.h"

typedef struct LookupParameters {
        const char *user_name;
        const char *group_name;
        union {
                uid_t uid;
                gid_t gid;
        };
        const char *service;
} LookupParameters;

static int build_user_json(const char *user_name, uid_t uid, JsonVariant **ret) {
        assert(user_name);
        assert(uid_is_valid(uid));
        assert(ret);

        return json_build(ret, JSON_BUILD_OBJECT(
                                   JSON_BUILD_PAIR("record", JSON_BUILD_OBJECT(
                                       JSON_BUILD_PAIR("userName", JSON_BUILD_STRING(user_name)),
                                       JSON_BUILD_PAIR("uid", JSON_BUILD_UNSIGNED(uid)),
                                       JSON_BUILD_PAIR("gid", JSON_BUILD_UNSIGNED(uid)),
                                       JSON_BUILD_PAIR("realName", JSON_BUILD_STRING("Dynamic User")),
                                       JSON_BUILD_PAIR("homeDirectory", JSON_BUILD_STRING("/")),
                                       JSON_BUILD_PAIR("shell", JSON_BUILD_STRING(NOLOGIN)),
                                       JSON_BUILD_PAIR("locked", JSON_BUILD_BOOLEAN(true)),
                                       JSON_BUILD_PAIR("service", JSON_BUILD_STRING("io.systemd.DynamicUser")),
                                       JSON_BUILD_PAIR("disposition", JSON_BUILD_STRING("dynamic"))))));
}

static bool user_match_lookup_parameters(LookupParameters *p, const char *name, uid_t uid) {
        assert(p);

        if (p->user_name && !streq(name, p->user_name))
                return false;

        if (uid_is_valid(p->uid) && uid != p->uid)
                return false;

        return true;
}

static int vl_method_get_user_record(Varlink *link, JsonVariant *parameters, VarlinkMethodFlags flags, void *userdata) {

        static const JsonDispatch dispatch_table[] = {
                { "uid",      JSON_VARIANT_UNSIGNED, json_dispatch_uid_gid,      offsetof(LookupParameters, uid),       0         },
                { "userName", JSON_VARIANT_STRING,   json_dispatch_const_string, offsetof(LookupParameters, user_name), JSON_SAFE },
                { "service",  JSON_VARIANT_STRING,   json_dispatch_const_string, offsetof(LookupParameters, service),   0         },
                {}
        };

        _cleanup_(json_variant_unrefp) JsonVariant *v = NULL;
        LookupParameters p = {
                .uid = UID_INVALID,
        };
        _cleanup_free_ char *found_name = NULL;
        uid_t found_uid = UID_INVALID, uid;
        Manager *m = userdata;
        const char *un;
        int r;

        assert(parameters);
        assert(m);

        r = json_dispatch(parameters, dispatch_table, NULL, 0, &p);
        if (r < 0)
                return r;

        if (!streq_ptr(p.service, "io.systemd.DynamicUser"))
                return varlink_error(link, "io.systemd.UserDatabase.BadService", NULL);

        if (uid_is_valid(p.uid))
                r = dynamic_user_lookup_uid(m, p.uid, &found_name);
        else if (p.user_name)
                r = dynamic_user_lookup_name(m, p.user_name, &found_uid);
        else {
                Iterator i;
                DynamicUser *d;

                HASHMAP_FOREACH(d, m->dynamic_users, i) {
                        r = dynamic_user_current(d, &uid);
                        if (r == -EAGAIN) /* not realized yet? */
                                continue;
                        if (r < 0)
                                return r;

                        if (!user_match_lookup_parameters(&p, d->name, uid))
                                continue;

                        if (v) {
                                r = varlink_notify(link, v);
                                if (r < 0)
                                        return r;

                                v = json_variant_unref(v);
                        }

                        r = build_user_json(d->name, uid, &v);
                        if (r < 0)
                                return r;
                }

                if (!v)
                        return varlink_error(link, "io.systemd.UserDatabase.NoRecordFound", NULL);

                return varlink_reply(link, v);
        }
        if (r == -ESRCH)
                return varlink_error(link, "io.systemd.UserDatabase.NoRecordFound", NULL);
        if (r < 0)
                return r;

        uid = uid_is_valid(found_uid) ? found_uid : p.uid;
        un = found_name ?: p.user_name;

        if (!user_match_lookup_parameters(&p, un, uid))
                return varlink_error(link, "io.systemd.UserDatabase.ConflictingRecordFound", NULL);

        r = build_user_json(un, uid, &v);
        if (r < 0)
                return r;

        return varlink_reply(link, v);
}

static int build_group_json(const char *group_name, gid_t gid, JsonVariant **ret) {
        assert(group_name);
        assert(gid_is_valid(gid));
        assert(ret);

        return json_build(ret, JSON_BUILD_OBJECT(
                                   JSON_BUILD_PAIR("record", JSON_BUILD_OBJECT(
                                       JSON_BUILD_PAIR("groupName", JSON_BUILD_STRING(group_name)),
                                       JSON_BUILD_PAIR("description", JSON_BUILD_STRING("Dynamic Group")),
                                       JSON_BUILD_PAIR("gid", JSON_BUILD_UNSIGNED(gid)),
                                       JSON_BUILD_PAIR("service", JSON_BUILD_STRING("io.systemd.DynamicUser")),
                                       JSON_BUILD_PAIR("disposition", JSON_BUILD_STRING("dynamic"))))));
    }

static bool group_match_lookup_parameters(LookupParameters *p, const char *name, gid_t gid) {
        assert(p);

        if (p->group_name && !streq(name, p->group_name))
                return false;

        if (gid_is_valid(p->gid) && gid != p->gid)
                return false;

        return true;
}

static int vl_method_get_group_record(Varlink *link, JsonVariant *parameters, VarlinkMethodFlags flags, void *userdata) {

        static const JsonDispatch dispatch_table[] = {
                { "gid",       JSON_VARIANT_UNSIGNED, json_dispatch_uid_gid,      offsetof(LookupParameters, gid),        0         },
                { "groupName", JSON_VARIANT_STRING,   json_dispatch_const_string, offsetof(LookupParameters, group_name), JSON_SAFE },
                { "service",   JSON_VARIANT_STRING,   json_dispatch_const_string, offsetof(LookupParameters, service),    0         },
                {}
        };

        _cleanup_(json_variant_unrefp) JsonVariant *v = NULL;
        LookupParameters p = {
                .gid = GID_INVALID,
        };
        _cleanup_free_ char *found_name = NULL;
        uid_t found_gid = GID_INVALID, gid;
        Manager *m = userdata;
        const char *gn;
        int r;

        assert(parameters);
        assert(m);

        r = json_dispatch(parameters, dispatch_table, NULL, 0, &p);
        if (r < 0)
                return r;

        if (!streq_ptr(p.service, "io.systemd.DynamicUser"))
                return varlink_error(link, "io.systemd.UserDatabase.BadService", NULL);

        if (gid_is_valid(p.gid))
                r = dynamic_user_lookup_uid(m, (uid_t) p.gid, &found_name);
        else if (p.group_name)
                r = dynamic_user_lookup_name(m, p.group_name, (uid_t*) &found_gid);
        else {
                DynamicUser *d;
                Iterator i;

                HASHMAP_FOREACH(d, m->dynamic_users, i) {
                        uid_t uid;

                        r = dynamic_user_current(d, &uid);
                        if (r == -EAGAIN)
                                continue;
                        if (r < 0)
                                return r;

                        if (!group_match_lookup_parameters(&p, d->name, (gid_t) uid))
                                continue;

                        if (v) {
                                r = varlink_notify(link, v);
                                if (r < 0)
                                        return r;

                                v = json_variant_unref(v);
                        }

                        r = build_group_json(d->name, (gid_t) uid, &v);
                        if (r < 0)
                                return r;
                }

                if (!v)
                        return varlink_error(link, "io.systemd.UserDatabase.NoRecordFound", NULL);

                return varlink_reply(link, v);
        }
        if (r == -ESRCH)
                return varlink_error(link, "io.systemd.UserDatabase.NoRecordFound", NULL);
        if (r < 0)
                return r;

        gid = gid_is_valid(found_gid) ? found_gid : p.gid;
        gn = found_name ?: p.group_name;

        if (!group_match_lookup_parameters(&p, gn, gid))
                return varlink_error(link, "io.systemd.UserDatabase.ConflictingRecordFound", NULL);

        r = build_group_json(gn, gid, &v);
        if (r < 0)
                return r;

        return varlink_reply(link, v);
}

static int vl_method_get_memberships(Varlink *link, JsonVariant *parameters, VarlinkMethodFlags flags, void *userdata) {

        static const JsonDispatch dispatch_table[] = {
                { "userName",  JSON_VARIANT_STRING, json_dispatch_const_string, offsetof(LookupParameters, user_name),  JSON_SAFE },
                { "groupName", JSON_VARIANT_STRING, json_dispatch_const_string, offsetof(LookupParameters, group_name), JSON_SAFE },
                { "service",   JSON_VARIANT_STRING, json_dispatch_const_string, offsetof(LookupParameters, service),    0         },
                {}
        };

        LookupParameters p = {};
        int r;

        assert(parameters);

        r = json_dispatch(parameters, dispatch_table, NULL, 0, &p);
        if (r < 0)
                return r;

        if (!streq_ptr(p.service, "io.systemd.DynamicUser"))
                return varlink_error(link, "io.systemd.UserDatabase.BadService", NULL);

        /* We don't support auxiliary groups with dynamic users. */
        return varlink_error(link, "io.systemd.UserDatabase.NoRecordFound", NULL);
}

int manager_varlink_init(Manager *m) {
        _cleanup_(varlink_server_unrefp) VarlinkServer *s = NULL;
        int r;

        assert(m);

        if (m->varlink_server)
                return 0;

        if (!MANAGER_IS_SYSTEM(m))
                return 0;

        r = varlink_server_new(&s, VARLINK_SERVER_ACCOUNT_UID);
        if (r < 0)
                return log_error_errno(r, "Failed to allocate varlink server object: %m");

        varlink_server_set_userdata(s, m);

        r = varlink_server_bind_method_many(
                        s,
                        "io.systemd.UserDatabase.GetUserRecord",  vl_method_get_user_record,
                        "io.systemd.UserDatabase.GetGroupRecord", vl_method_get_group_record,
                        "io.systemd.UserDatabase.GetMemberships", vl_method_get_memberships);
        if (r < 0)
                return log_error_errno(r, "Failed to register varlink methods: %m");

        if (!MANAGER_IS_TEST_RUN(m)) {
                (void) mkdir_p_label("/run/systemd/userdb", 0755);

                r = varlink_server_listen_address(s, "/run/systemd/userdb/io.systemd.DynamicUser", 0666);
                if (r < 0)
                        return log_error_errno(r, "Failed to bind to varlink socket: %m");
        }

        r = varlink_server_attach_event(s, m->event, SD_EVENT_PRIORITY_NORMAL);
        if (r < 0)
                return log_error_errno(r, "Failed to attach varlink connection to event loop: %m");

        m->varlink_server = TAKE_PTR(s);
        return 0;
}

void manager_varlink_done(Manager *m) {
        assert(m);

        m->varlink_server = varlink_server_unref(m->varlink_server);
}
