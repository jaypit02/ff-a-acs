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

static uint32_t retrieve_align_hint_err_check(ffa_memory_handle_t handle, uint32_t fid,
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
    if (fid == FFA_MEM_LEND_64)
        val_ffa_mem_retrieve_64(&payload);
    else
        val_ffa_mem_retrieve_32(&payload);

    if ((payload.fid != FFA_ERROR_32) || (payload.arg2 != FFA_ERROR_INVALID_PARAMETERS))
    {
        LOG(ERROR,
            "\tRelayer must return error if a reserved value is specified by the receiver err %x\n",
                payload.arg2, 0);
        status =  VAL_ERROR_POINT(1);
        if (payload.fid == FFA_MEM_RETRIEVE_RESP_32)
        {
            relinquish_memory(handle, tx_buf, receiver);
        }
   }

    return status;
}

static uint32_t retrieve_align_hint_check(ffa_memory_handle_t handle, uint32_t fid,
                mb_buf_t mb, ffa_endpoint_id_t sender, ffa_endpoint_id_t receiver,
                ffa_memory_region_flags_t flags)
{
    mem_region_init_t mem_region_init;
    uint32_t msg_size;
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;
    uint32_t align_size = 0x2000;
    uint8_t *ptr;
    struct ffa_memory_region *memory_region;
    struct ffa_composite_memory_region *composite;

    mem_region_init.memory_region = mb.send;
    mem_region_init.sender = sender;
    mem_region_init.receiver = receiver;
    mem_region_init.tag = 0;
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
    if (fid == FFA_MEM_LEND_64)
        val_ffa_mem_retrieve_64(&payload);
    else
        val_ffa_mem_retrieve_32(&payload);

    if ((payload.fid == FFA_ERROR_32) || (payload.arg2 == FFA_ERROR_DENIED))
    {
        LOG(TEST,
            "\tNot possible to allocate the address ranges specified by the receiver\n", 0, 0);
    }
    else if (payload.fid == FFA_MEM_RETRIEVE_RESP_32)
    {
        memory_region = (struct ffa_memory_region *)mb.recv;
        composite = ffa_memory_region_get_composite(memory_region, 0);
        ptr = (uint8_t *)composite->constituents[0].address;
        /* Check the retrieved address is 8KB aligned or not */
        if ((uint64_t)ptr % align_size != 0)
        {
            LOG(ERROR, "\tRetrieved address is not algined as specified by the receiver\n", 0, 0);
            status = VAL_ERROR_POINT(2);
        }

        relinquish_memory(handle, mb.send, receiver);
    }
    else
        status = VAL_ERROR_POINT(3);

    return status;
}

uint32_t lend_retrieve_align_hint_check_server(ffa_args_t args)
{
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;
    ffa_endpoint_id_t sender = args.arg1 & 0xffff;
    ffa_endpoint_id_t receiver = (args.arg1 >> 16) & 0xffff;
    uint32_t fid = (uint32_t)args.arg4;
    mb_buf_t mb;
    uint64_t size = PAGE_SIZE_4K;
    ffa_memory_handle_t handle;
    ffa_memory_region_flags_t flags = 0;

    mb.send = val_memory_alloc(size);
    mb.recv = val_memory_alloc(size);
    if (mb.send == NULL || mb.recv == NULL)
    {
        LOG(ERROR, "\tFailed to allocate RxTx buffer\n", 0, 0);
        status = VAL_ERROR_POINT(4);
        goto free_memory;
    }

    /* Map TX and RX buffers */
    if (val_rxtx_map_64((uint64_t)mb.send, (uint64_t)mb.recv, (uint32_t)(size/PAGE_SIZE_4K)))
    {
        LOG(ERROR, "\tRxTx Map failed\n", 0, 0);
        status = VAL_ERROR_POINT(5);
        goto free_memory;
    }

    /* Wait for the message. */
    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload = val_resp_client_fn_direct((uint32_t)args.arg3, 0, 0, 0, 0, 0);
    if (payload.fid != FFA_MSG_SEND_DIRECT_REQ_64)
    {
        LOG(ERROR, "\tDirect request failed, fid=0x%x, err 0x%x\n",
                  payload.fid, payload.arg2);
        status =  VAL_ERROR_POINT(6);
        goto rxtx_unmap;
    }

    handle = payload.arg3;
    /* Bit[9:5] - Address range alignment hint.
     *          - Bit[9]: Hint valid flag.
     *            b`0 Relayer must choose the alignment boundary. Bits[8:5] are reserved and MBZ.
     */
    /* TEST-1: Set bit[9]=0, bit[8:5]=0x8 and check for error status code */
    flags = VAL_SET_BITS(flags, 5, 5, 0x8);
    status = retrieve_align_hint_err_check(handle, fid, mb.send, receiver,
                        sender, flags);
    if (status)
        goto rxtx_unmap;

    /* Bit[9:5] - Address range alignment hint.
     *          - Bit[9]: Hint valid flag.
     *            b`1 Relayer must use the alignment boundary specified in Bits[8:5].
     *          - Bit[8:5]: If the value in this field is n, then the address ranges
     *                      must be aligned to the 2*n x 4KB boundary.
     */
    /* TEST-2: Set bit[9]=1, bit[8:5]=0x1 and check for 2*1 x 4KB = 8KB alignment boundary. */
    flags = VAL_SET_BITS(flags, 5, 5, 0x11);
    status = retrieve_align_hint_check(handle, fid, mb, receiver,
                        sender, flags);
rxtx_unmap:
    if (val_rxtx_unmap(sender))
    {
        LOG(ERROR, "RXTX_UNMAP failed\n", 0, 0);
        status = status ? status : VAL_ERROR_POINT(7);
    }

free_memory:
    if (val_memory_free(mb.recv, size) || val_memory_free(mb.send, size))
    {
        LOG(ERROR, "\tfree_rxtx_buffers failed\n", 0, 0);
        status = status ? status : VAL_ERROR_POINT(8);
    }

    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 =  ((uint32_t)sender << 16) | receiver;
    val_ffa_msg_send_direct_resp_64(&payload);
    if (payload.fid == FFA_ERROR_32)
    {
        LOG(ERROR, "\tDirect response failed err %x\n", payload.arg2, 0);
        status = status ? status : VAL_ERROR_POINT(9);
    }

    return status;
}
