/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 * @brief IronBee
 *
 * @author Nick LeRoy <nleroy@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/engine.h>
#include <ironbee/mpool.h>
#include <ironbee/bytestr.h>
#include <ironbee/debug.h>
#include <ironbee/rule_engine.h>

#include "ironbee_private.h"
#include "ironbee_core_private.h"

static ib_status_t op_streq_create(ib_mpool_t *mp,
                                   const char *data,
                                   ib_operator_inst_t *op_inst)
{
    // @todo
    op_inst->data = strdup(data);
    return IB_OK;
}
static ib_status_t op_streq_destroy(ib_operator_inst_t *inst)
{
    // @todo
    return IB_OK;
}
static ib_status_t op_streq_execute(void *data,
                                    ib_field_t *field,
                                    ib_num_t *result)
{
    // @todo
    const char *cstr = (const char *)data;
    if (field->type==IB_FTYPE_NULSTR) {
        const char *fval = ib_field_value_nulstr( field );
        *result = (strcmp(fval,cstr) == 0);
    }
    else if (field->type==IB_FTYPE_BYTESTR) {
        char buf[256];
        ib_bytestr_t *value = ib_field_value_bytestr(field);
        strncpy(buf, (const char*)ib_bytestr_ptr(value), sizeof(buf) );
        buf[sizeof(buf)-1] = '\0';
        *result = (strcmp(buf,cstr) == 0);
    }
    else {
        return IB_EINVAL;
    }

    return IB_OK;
}

ib_status_t ib_core_operators_init(ib_engine_t *ib, ib_module_t *mod)
{
    IB_FTRACE_INIT(ib_core_operators_init);
    ib_status_t rc;

    rc = ib_operator_register(ib, "@streq",
                              op_streq_create,
                              op_streq_destroy,
                              op_streq_execute);
    if (rc != IB_OK) {
        
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}
