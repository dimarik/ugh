#include <ugh/ugh.h>

typedef struct
{
	ugh_template_t session_host;
	ugh_template_t friends_host;
	ugh_template_t wall_host;
	ugh_template_t logger_host;
} ugh_module_example_conf_t;

extern ugh_module_t ugh_module_example;

int ugh_module_example_handle(ugh_client_t *c, void *data, strp body)
{
	ugh_module_example_conf_t *conf = data; /* get module conf */

	strp cookie_sid = ugh_client_cookie_get(c, "sid", 3);

	/* order one session subrequest */

	strp session_host = ugh_template_execute(&conf->session_host, c);
	ugh_subreq_t *r_session = ugh_subreq_add(c, session_host->data, session_host->size, UGH_SUBREQ_WAIT);
	ugh_subreq_set_body(r_session, cookie_sid->data, cookie_sid->size);
	ugh_subreq_run(r_session);

	/* wait for it (do smth in different coroutine while it is being downloaded) */

	ugh_subreq_wait(c);

	if (0 == cookie_sid->size)
	{
		ugh_client_header_out_set(c, "Set-Cookie", sizeof("Set-Cookie") - 1, r_session->body.data, r_session->body.size);

		return UGH_HTTP_OK;
	}

	if (0 == r_session->body.size)
	{
		body->data = "Please login [login form]";
		body->size = sizeof("Please login [login form]") - 1;

		return UGH_HTTP_OK;
	}

	/* order friends subrequest */

	strp friends_host = ugh_template_execute(&conf->friends_host, c);
	ugh_subreq_t *r_friends = ugh_subreq_add(c, friends_host->data, friends_host->size, UGH_SUBREQ_WAIT);
	ugh_subreq_set_body(r_friends, r_session->body.data, r_session->body.size);
	ugh_subreq_run(r_friends);

	/* order wall subrequest */

	strp wall_host = ugh_template_execute(&conf->wall_host, c);
	ugh_subreq_t *r_wall = ugh_subreq_add(c, wall_host->data, wall_host->size, UGH_SUBREQ_WAIT);
	ugh_subreq_set_body(r_wall, r_session->body.data, r_session->body.size);
	ugh_subreq_run(r_wall);

	/* order logger subrequest, but tell ugh not to wait for its result */

	strp logger_host = ugh_template_execute(&conf->logger_host, c);
	ugh_subreq_t *r_logger = ugh_subreq_add(c, logger_host->data, logger_host->size, 0);
	ugh_subreq_set_body(r_logger, r_session->body.data, r_session->body.size);
	ugh_subreq_run(r_logger);

	/* wait for two ordered subrequests */

	ugh_subreq_wait(c);

	/* set body */

	body->size = 0;
	body->data = aux_pool_malloc(c->pool, r_friends->body.size + r_wall->body.size);
	if (NULL == body->data) return UGH_HTTP_INTERNAL_SERVER_ERROR;

	body->size += aux_cpymsz(body->data + body->size, r_friends->body.data, r_friends->body.size);
	body->size += aux_cpymsz(body->data + body->size, r_wall->body.data, r_wall->body.size);

	/* return 200 OK status */

	return UGH_HTTP_OK;
}

int ugh_module_example_init(ugh_config_t *cfg, void *data)
{
	ugh_module_example_conf_t *conf = data;

	log_info("ugh_module_example_init (called for each added handle, each time server is starting), conf=%p", &conf);

	return 0;
}

int ugh_module_example_free(ugh_config_t *cfg, void *data)
{
	log_info("ugh_module_example_free (called for each added handle, each time server is stopped)");

	return 0;
}

int ugh_command_example(ugh_config_t *cfg, int argc, char **argv, ugh_command_t *cmd)
{
	ugh_module_example_conf_t *conf;
	
	conf = aux_pool_malloc(cfg->pool, sizeof(*conf));
	if (NULL == conf) return -1;

	ugh_module_handle_add(ugh_module_example, conf, ugh_module_example_handle);

	return 0;
}

static ugh_command_t ugh_module_example_cmds [] =
{
	ugh_make_command(example),
	ugh_make_command_template(example_session_host, ugh_module_example_conf_t, session_host),
	ugh_make_command_template(example_friends_host, ugh_module_example_conf_t, friends_host),
	ugh_make_command_template(example_wall_host   , ugh_module_example_conf_t, wall_host),
	ugh_make_command_template(example_logger_host , ugh_module_example_conf_t, logger_host),
	ugh_null_command
};

ugh_module_t ugh_module_example = 
{
	ugh_module_example_cmds,
	ugh_module_example_init,
	ugh_module_example_free
};

