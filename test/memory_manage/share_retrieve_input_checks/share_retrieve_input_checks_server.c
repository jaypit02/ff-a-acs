/*
 * Copyright (c) 2021, Arm Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "test_database.h"

static void relinquish_memory(ffa_memory_handle_t handle, void *tx_buf, ffa_endpoint_id_t receiver)
{
    ffa_args_t payload;

    /* relinquish the memory and notify the sender. */
    ffa_mem_relinquish_init((struct ffa_mem_relinquish *)tx_buf, handle, 0, receiver, 0x1);
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

static uint32_t retrieve_zero_flag_check(ffa_memory_handle_t handle, uint32_t fid,
                void *tx_buf, ffa_endpoint_id_t sender, ffa_endpoint_id_t receiver,
                ffa_memory_region_flags_t flags)
{
    mem_region_init_t mem_region_init;
    uint32_t msg_size;
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;

    mem_region_init.memory_region = tx_buf;
    mem_region_init.sender = sender;
    mem_region_init.receiver = receiver;
    mem_region_init.tag = 0;
    /* Zero memory before retrieval flag: MBZ in a transaction to share a memory region,
     * else the Relayer must return INVALID_PARAMETER.
     *
     * Zero memory after relinquish flag: MBZ in a transaction to share a memory region,
     * else the Relayer must return INVALID_PARAMETER.
     */
    mem_region_init.flags = flags;
    mem_region_init.data_access = FFA_DATA_ACCESS_RW;
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
    mem_region_init.type = FFA_MEMORY_NORMAL_MEM;
    mem_region_init.cacheability = FFA_MEMORY_CACHE_NON_CACHEABLE;
    mem_region_init.shareability = FFA_MEMORY_OUTER_SHAREABLE;
    msg_size = val_ffa_memory_retrieve_request_init(&mem_region_init, handle);

    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = msg_size;
    payload.arg2 = msg_size;
    if (fid == FFA_MEM_SHARE_64)
        val_ffa_mem_retrieve_64(&payload);
    else
        val_ffa_mem_retrieve_32(&payload);

    if ((payload.fid != FFA_ERROR_32) || (payload.arg2 != FFA_ERROR_INVALID_PARAMETERS))
    {
        if (flags == FFA_MEMORY_REGION_FLAG_CLEAR)
        {
            LOG(ERROR,
                "\tRelayer must return error if zero memory before retrieval flag is set\n",
                0, 0);
        }
        else if (flags == FFA_MEMORY_REGION_FLAG_CLEAR_RELINQUISH)
        {
            LOG(ERROR,
                "\tRelayer must return error if zero memory after relinquish flag is set\n",
                0, 0);
        }
        status =  VAL_ERROR_POINT(1);
        if (payload.fid == FFA_MEM_RETRIEVE_RESP_32)
        {
            relinquish_memory(handle, tx_buf, receiver);
        }
   }

    return status;
}

static uint32_t retrieve_with_invalid_cache_attr_check(ffa_memory_handle_t handle, uint32_t fid,
                void *tx_buf, ffa_endpoint_id_t sender, ffa_endpoint_id_t receiver)
{
    mem_region_init_t mem_region_init;
    uint32_t msg_size;
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;

    mem_region_init.memory_region = tx_buf;
    mem_region_init.sender = sender;
    mem_region_init.receiver = receiver;
    mem_region_init.tag = 0;
    mem_region_init.flags = 0;
    mem_region_init.data_access = FFA_DATA_ACCESS_RW;
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
    mem_region_init.type = FFA_MEMORY_NORMAL_MEM;
    /* Cacheability attribute precedence rules are as follows:
     * Non-cacheable < Write-Back Cacheable.
     * The Relayer must return the DENIED error code if the validation fails.
     */
    mem_region_init.cacheability = FFA_MEMORY_CACHE_WRITE_BACK;
    mem_region_init.shareability = FFA_MEMORY_OUTER_SHAREABLE;
    msg_size = val_ffa_memory_retrieve_request_init(&mem_region_init, handle);

    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = msg_size;
    payload.arg2 = msg_size;
    if (fid == FFA_MEM_SHARE_64)
        val_ffa_mem_retrieve_64(&payload);
    else
        val_ffa_mem_retrieve_32(&payload);

    if ((payload.fid != FFA_ERROR_32) || (payload.arg2 != FFA_ERROR_DENIED))
    {
        LOG(ERROR, "Relayer must return denied for cache attribute mismatch %x\n", payload.arg2, 0);
        status =  VAL_ERROR_POINT(2);
        if (payload.fid == FFA_MEM_RETRIEVE_RESP_32)
        {
            relinquish_memory(handle, tx_buf, receiver);
        }
    }

    return status;
}

static uint32_t retrieve_with_invalid_total_length_check(ffa_memory_handle_t handle, uint32_t fid,
                void *tx_buf, ffa_endpoint_id_t sender, ffa_endpoint_id_t receiver)
{
    mem_region_init_t mem_region_init;
    uint32_t msg_size;
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;

    mem_region_init.memory_region = tx_buf;
    mem_region_init.sender = sender;
    mem_region_init.receiver = receiver;
    mem_region_init.tag = 0;
    mem_region_init.flags = 0;
    mem_region_init.data_access = FFA_DATA_ACCESS_RW;
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
    mem_region_init.type = FFA_MEMORY_NORMAL_MEM;
    mem_region_init.cacheability = FFA_MEMORY_CACHE_NON_CACHEABLE;
    mem_region_init.shareability = FFA_MEMORY_OUTER_SHAREABLE;
    msg_size = val_ffa_memory_retrieve_request_init(&mem_region_init, handle);

    val_memset(&payload, 0, sizeof(ffa_args_t));
    /* Relayer must validate the Total length input parameter to ensure
     * that the length of the transaction descriptordoes not exceed the
     * size of the buffer it has been populated in. Must return INVALID_PARAMETERS
     * incase of an error.*/

    payload.arg1 = PAGE_SIZE_4K + 1;
    payload.arg2 = msg_size;
    if (fid == FFA_MEM_SHARE_64)
        val_ffa_mem_retrieve_64(&payload);
    else
        val_ffa_mem_retrieve_32(&payload);

    if ((payload.fid != FFA_ERROR_32) || (payload.arg2 != FFA_ERROR_INVALID_PARAMETERS))
    {
        LOG(ERROR, "\ttotal length must be lesser than transaction buffer size\n", 0, 0);
        LOG(ERROR, "\tRelayer must return %x instead of %x\n",
                  FFA_ERROR_INVALID_PARAMETERS, payload.arg2);
        status =  VAL_ERROR_POINT(3);
        if (payload.fid == FFA_MEM_RETRIEVE_RESP_32)
        {
            relinquish_memory(handle, tx_buf, receiver);
        }
    }

    return status;
}

static uint32_t retrieve_with_invalid_sender_id(ffa_memory_handle_t handle, uint32_t fid,
                void *tx_buf, ffa_endpoint_id_t sender, ffa_endpoint_id_t receiver)
{
    mem_region_init_t mem_region_init;
    uint32_t msg_size;
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;

    mem_region_init.memory_region = tx_buf;
    mem_region_init.sender = sender;
    mem_region_init.receiver = receiver;
    mem_region_init.tag = 0;
    mem_region_init.flags = 0;
    mem_region_init.data_access = FFA_DATA_ACCESS_RW;
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
    mem_region_init.type = FFA_MEMORY_NORMAL_MEM;
    mem_region_init.cacheability = FFA_MEMORY_CACHE_NON_CACHEABLE;
    mem_region_init.shareability = FFA_MEMORY_OUTER_SHAREABLE;
    msg_size = val_ffa_memory_retrieve_request_init(&mem_region_init, handle);

    val_memset(&payload, 0, sizeof(ffa_args_t));
    /* Must validate the Sender endpoint ID field in the transaction
     * descriptor to ensure that the Sender is theOwner of the memory
     * region or the proxy endpoint acting on behalf of a stream endpoint.
     * Must returnDENIED in case of an error. Passing sender id = SP3.
     */
    payload.arg1 = msg_size;
    payload.arg2 = msg_size;
    if (fid == FFA_MEM_SHARE_64)
        val_ffa_mem_retrieve_64(&payload);
    else
        val_ffa_mem_retrieve_32(&payload);

    if ((payload.fid != FFA_ERROR_32) || (payload.arg2 != FFA_ERROR_INVALID_PARAMETERS))
    {
        LOG(ERROR, "\ttotal length must be lesser than transaction buffer size\n", 0, 0);
        LOG(ERROR, "\tRelayer must return %x instead of %x\n",
                  FFA_ERROR_INVALID_PARAMETERS, payload.arg2);
        status =  VAL_ERROR_POINT(4);
        if (payload.fid == FFA_MEM_RETRIEVE_RESP_32)
        {
            relinquish_memory(handle, tx_buf, receiver);
        }
    }

    return status;
}

static uint32_t retrieve_with_invalid_inst_perm(ffa_memory_handle_t handle, uint32_t fid,
                void *tx_buf, ffa_endpoint_id_t sender, ffa_endpoint_id_t receiver)
{
    mem_region_init_t mem_region_init;
    uint32_t msg_size;
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;

    mem_region_init.memory_region = tx_buf;
    mem_region_init.sender = sender;
    mem_region_init.receiver = receiver;
    mem_region_init.tag = 0;
    mem_region_init.flags = 0;
    mem_region_init.data_access = FFA_DATA_ACCESS_RW;
    /* Instruction access permission. Bits[3:2] must be set to b'00 as follows:
     * By the Borrower in an invocation of the FFA_MEM_RETRIEVE_REQ ABI. */
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NX;
    mem_region_init.type = FFA_MEMORY_NORMAL_MEM;
    mem_region_init.cacheability = FFA_MEMORY_CACHE_NON_CACHEABLE;
    mem_region_init.shareability = FFA_MEMORY_OUTER_SHAREABLE;
    msg_size = val_ffa_memory_retrieve_request_init(&mem_region_init, handle);

    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = msg_size;
    payload.arg2 = msg_size;
    if (fid == FFA_MEM_SHARE_64)
        val_ffa_mem_retrieve_64(&payload);
    else
        val_ffa_mem_retrieve_32(&payload);

    if ((payload.fid != FFA_ERROR_32) || (payload.arg2 != FFA_ERROR_INVALID_PARAMETERS))
    {
        LOG(ERROR, "\tMem retrieve request must fail for non-zero value of inst perm\n", 0, 0);
        LOG(ERROR, "\tRelayer must return %x instead of %x\n",
                  FFA_ERROR_INVALID_PARAMETERS, payload.arg2);
        status =  VAL_ERROR_POINT(5);
        if (payload.fid == FFA_MEM_RETRIEVE_RESP_32)
        {
            relinquish_memory(handle, tx_buf, receiver);
        }
    }

    return status;
}

uint32_t share_retrieve_input_checks_server(ffa_args_t args)
{
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;
    ffa_endpoint_id_t sender = args.arg1 & 0xffff;
    ffa_endpoint_id_t receiver = (args.arg1 >> 16) & 0xffff;
    uint32_t fid = (uint32_t)args.arg4;
    mb_buf_t mb;
    uint64_t size = PAGE_SIZE_4K;
    ffa_memory_handle_t handle;

    mb.send = val_memory_alloc(size);
    mb.recv = val_memory_alloc(size);
    if (mb.send == NULL || mb.recv == NULL)
    {
        LOG(ERROR, "\tFailed to allocate RxTx buffer\n", 0, 0);
        status = VAL_ERROR_POINT(6);
        goto free_memory;
    }

    /* Map TX and RX buffers */
    if (val_rxtx_map_64((uint64_t)mb.send, (uint64_t)mb.recv, (uint32_t)(size/PAGE_SIZE_4K)))
    {
        LOG(ERROR, "\tRxTx Map failed\n", 0, 0);
        status = VAL_ERROR_POINT(7);
        goto free_memory;
    }

    /* Wait for the message. */
    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload = val_resp_client_fn_direct((uint32_t)args.arg3, 0, 0, 0, 0, 0);
    if (payload.fid != FFA_MSG_SEND_DIRECT_REQ_64)
    {
        LOG(ERROR, "\tDirect request failed, fid=0x%x, err 0x%x\n",
                  payload.fid, payload.arg2);
        status =  VAL_ERROR_POINT(8);
        goto rxtx_unmap;
    }

    handle = payload.arg3;

    status = retrieve_zero_flag_check(handle, fid, mb.send, receiver,
                        sender, FFA_MEMORY_REGION_FLAG_CLEAR);
    if (status)
        goto rxtx_unmap;

    status = retrieve_zero_flag_check(handle, fid, mb.send, receiver,
                        sender, FFA_MEMORY_REGION_FLAG_CLEAR_RELINQUISH);
    if (status)
        goto rxtx_unmap;

    status = retrieve_with_invalid_cache_attr_check(handle, fid, mb.send, receiver, sender);
    if (status)
        goto rxtx_unmap;

    status = retrieve_with_invalid_total_length_check(handle, fid, mb.send, receiver, sender);
    if (status)
        goto rxtx_unmap;

    status = retrieve_with_invalid_sender_id(handle, fid, mb.send, receiver, val_get_endpoint_id(SP3));
    if (status)
        goto rxtx_unmap;

    status = retrieve_with_invalid_inst_perm(handle, fid, mb.send, receiver, val_get_endpoint_id(SP3));

rxtx_unmap:
    if (val_rxtx_unmap(sender))
    {
        LOG(ERROR, "RXTX_UNMAP failed\n", 0, 0);
        status = status ? status : VAL_ERROR_POINT(9);
    }

free_memory:
    if (val_memory_free(mb.recv, size) || val_memory_free(mb.send, size))
    {
        LOG(ERROR, "\tfree_rxtx_buffers failed\n", 0, 0);
        status = status ? status : VAL_ERROR_POINT(10);
    }

    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 =  ((uint32_t)sender << 16) | receiver;
    val_ffa_msg_send_direct_resp_64(&payload);
    if (payload.fid == FFA_ERROR_32)
    {
        LOG(ERROR, "\tDirect response failed err %x\n", payload.arg2, 0);
        status = status ? status : VAL_ERROR_POINT(11);
    }

    return status;
}
