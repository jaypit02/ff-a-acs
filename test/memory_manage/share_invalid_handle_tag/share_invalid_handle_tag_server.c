/*
 * Copyright (c) 2021, Arm Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "test_database.h"

static void mem_relinquish(ffa_memory_handle_t handle, void *tx_buf, ffa_endpoint_id_t sender)
{
    ffa_args_t payload;

    /* relinquish the memory and notify the sender. */
    ffa_mem_relinquish_init((struct ffa_mem_relinquish *)tx_buf, handle, 0, sender, 0x1);
    val_memset(&payload, 0, sizeof(ffa_args_t));
    val_ffa_mem_relinquish(&payload);
    if (payload.fid == FFA_ERROR_32)
    {
        LOG(ERROR, "\tMem relinquish failed err %x\n", payload.arg2, 0);
    }
    if (val_rx_release())
    {
        LOG(ERROR, "\tval_rx_release failed\n", 0, 0);
    }
}

static uint32_t mem_share_invalid_handle_tag_check(ffa_memory_handle_t handle, uint32_t fid,
                uint32_t tag, void *tx_buf, ffa_endpoint_id_t sender, ffa_endpoint_id_t receiver)
{
    mem_region_init_t mem_region_init;
    uint32_t msg_size;
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;

    /* MEM_SHARE: Relayer must ensure that the handle was allocated to the owner
     * specified in the sender endpoint ID field of the transaction descriptor
     */
    mem_region_init.memory_region = tx_buf;
    mem_region_init.sender = sender;
    mem_region_init.receiver = receiver;
    mem_region_init.tag = tag;
    mem_region_init.flags = 0;
    mem_region_init.data_access = FFA_DATA_ACCESS_RW;
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
    mem_region_init.type = FFA_MEMORY_NORMAL_MEM;
    mem_region_init.cacheability = FFA_MEMORY_CACHE_WRITE_BACK;
    mem_region_init.shareability = FFA_MEMORY_OUTER_SHAREABLE;
    /* Pass invalid handle from the receiver and check for error status code */
    msg_size = val_ffa_memory_retrieve_request_init(&mem_region_init, (handle-0xF));

    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = msg_size;
    payload.arg2 = msg_size;
    if (fid == FFA_MEM_SHARE_64)
        val_ffa_mem_retrieve_64(&payload);
    else
        val_ffa_mem_retrieve_32(&payload);

    if ((payload.fid != FFA_ERROR_32) || (payload.arg2 != FFA_ERROR_INVALID_PARAMETERS))
    {
        LOG(ERROR, "\tRelayer must return error for invalid handle %x\n", payload.arg2, 0);
        status =  VAL_ERROR_POINT(1);
        if (payload.fid == FFA_MEM_RETRIEVE_RESP_32)
        {
            mem_relinquish(handle, tx_buf, receiver);
        }
        goto err;
    }

    /* Pass invalid tag from the receiver and check for error status code */
    mem_region_init.tag = tag - 0xff;
    msg_size = val_ffa_memory_retrieve_request_init(&mem_region_init, handle);

    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = msg_size;
    payload.arg2 = msg_size;
    if (fid == FFA_MEM_LEND_64)
        val_ffa_mem_retrieve_64(&payload);
    else
        val_ffa_mem_retrieve_32(&payload);

    if ((payload.fid != FFA_ERROR_32) || (payload.arg2 != FFA_ERROR_INVALID_PARAMETERS))
    {
        LOG(ERROR, "\tRelayer must return error for invalid tag err %x\n",
                                payload.arg2, 0);
        status =  VAL_ERROR_POINT(2);
        if (payload.fid == FFA_MEM_RETRIEVE_RESP_32)
        {
            mem_relinquish(handle, tx_buf, receiver);
        }
    }

err:
    return status;
}

uint32_t share_invalid_handle_tag_server(ffa_args_t args)
{
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;
    ffa_endpoint_id_t sender = args.arg1 & 0xffff;
    ffa_endpoint_id_t receiver = (args.arg1 >> 16) & 0xffff;
    uint32_t fid = (uint32_t)args.arg4;
    uint32_t tag;
    mb_buf_t mb;
    uint64_t size = 0x1000;
    ffa_memory_handle_t handle;

    mb.send = val_memory_alloc(size);
    mb.recv = val_memory_alloc(size);
    if (mb.send == NULL || mb.recv == NULL)
    {
        LOG(ERROR, "\tFailed to allocate RxTx buffer\n", 0, 0);
        status = VAL_ERROR_POINT(3);
        goto free_memory;
    }

    /* Map TX and RX buffers */
    if (val_rxtx_map_64((uint64_t)mb.send, (uint64_t)mb.recv, (uint32_t)(size/PAGE_SIZE_4K)))
    {
        LOG(ERROR, "\tRxTx Map failed\n", 0, 0);
        status = VAL_ERROR_POINT(4);
        goto free_memory;
    }

    /* Wait for the message. */
    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload = val_resp_client_fn_direct((uint32_t)args.arg3, 0, 0, 0, 0, 0);
    if (payload.fid != FFA_MSG_SEND_DIRECT_REQ_64)
    {
        LOG(ERROR, "\tDirect request failed, fid=0x%x, err 0x%x\n",
                  payload.fid, payload.arg2);
        status =  VAL_ERROR_POINT(5);
        goto rxtx_unmap;
    }

    handle = payload.arg3;
    tag = (uint32_t)payload.arg4;
    status = mem_share_invalid_handle_tag_check(handle, fid, tag, mb.send, receiver, sender);

rxtx_unmap:
    if (val_rxtx_unmap(sender))
    {
        LOG(ERROR, "RXTX_UNMAP failed\n", 0, 0);
        status = status ? status : VAL_ERROR_POINT(6);
    }

free_memory:
    if (val_memory_free(mb.recv, size) || val_memory_free(mb.send, size))
    {
        LOG(ERROR, "\tval_mem_free failed\n", 0, 0);
        status = status ? status : VAL_ERROR_POINT(7);
    }

    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 =  ((uint32_t)sender << 16) | receiver;
    val_ffa_msg_send_direct_resp_64(&payload);
    if (payload.fid == FFA_ERROR_32)
    {
        LOG(ERROR, "\tDirect response failed err %x\n", payload.arg2, 0);
        status = status ? status : VAL_ERROR_POINT(8);
    }

    return status;
}
