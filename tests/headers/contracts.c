/*
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 tanglinjie
 */
/* Compile-only usage checks; no provider implementation or runtime claim. */
#include <agent.h>
#include <agent/context.h>
#include <agent/model.h>
#include <agent/skill.h>
#include <agent/transport.h>

static const agent_string_view_t literal = AGENT_SV_LITERAL("sample");
static const agent_limits_t limits = AGENT_LIMITS_DEFAULT;
typedef char policy_zero_must_deny[(AGENT_POLICY_DENY == 0) ? 1 : -1];
typedef char projected_messages_must_be_positive[(AGENT_MAX_PROJECTED_MESSAGES > 0u) ? 1 : -1];

static agent_error_t sample_model(void* context, const agent_model_request_t* request,
                                       const agent_model_sink_t* sink)
{
    (void)context;
    (void)request;
    return sink->text(sink->context, agent_string_view(literal.data, literal.size));
}

static agent_error_t sample_tool(void* state, const agent_tool_context_t* context,
                                      const agent_text_sink_t* output)
{
    (void)state;
    (void)context;
    return output->write(output->context, agent_string_view(literal.data, literal.size));
}

static agent_error_t sample_context(void* state, const agent_context_request_t* request,
                                         const agent_text_sink_t* output)
{
    (void)state;
    (void)request;
    return output->write(output->context, literal);
}

static agent_policy_decision_t sample_policy(void* state, const agent_policy_request_t* request)
{
    (void)state;
    (void)request;
    return AGENT_POLICY_DENY;
}

static agent_error_t sample_http(void* state, const agent_http_request_t* request,
                                      const agent_http_sink_t* sink)
{
    (void)state;
    (void)request;
    (void)sink;
    return AGENT_ERROR_NOT_SUPPORTED;
}

int header_contract_fixture(agent_t* agent, agent_model_workspace_t* model_workspace);

int header_contract_fixture(agent_t* agent, agent_model_workspace_t* model_workspace)
{
    static agent_workspace_t core_workspace;
    static const agent_model_ops_t model_ops = {sample_model, NULL};
    static const agent_transport_ops_t http_ops = {sample_http};
    agent_transport_t transport = {&http_ops, NULL};
    agent_tool_t tool = {AGENT_SV_LITERAL("sample"),
                         AGENT_SV_LITERAL("description"),
                         AGENT_SV_LITERAL("{\"type\":\"object\"}"),
                         AGENT_SV_LITERAL("group"),
                         AGENT_SV_LITERAL("category"),
                         AGENT_TOOL_READ_ONLY,
                         NULL,
                         sample_tool,
                         NULL};
    agent_context_provider_t context = {AGENT_SV_LITERAL("state"), 0, false, sample_context, NULL,
                                         128u, AGENT_CONTEXT_REFERENCE};
    agent_model_t* model = NULL;
    agent_event_type_t event_type = AGENT_EVENT_TURN_BEGIN;
    agent_event_t event;
    agent_error_t (*init_fn)(agent_t**, agent_workspace_t*, const agent_config_t*) = agent_init;
    (void)core_workspace;
    (void)init_fn;
    agent_error_t status = agent_model_init(&model, model_workspace, &model_ops, NULL);
    if (limits.max_history_turns != 0u)
    {
        return AGENT_ERROR;
    }
    if (status != AGENT_OK)
    {
        return status;
    }
    (void)transport;
    event.type = event_type;
    (void)event.type;
    (void)agent_set_model(agent, model);
    (void)agent_register_tool(agent, &tool);
    (void)agent_register_context(agent, &context);
    (void)agent_set_policy_callback(agent, sample_policy, NULL);
    return agent_set_limits(agent, &limits);
}
