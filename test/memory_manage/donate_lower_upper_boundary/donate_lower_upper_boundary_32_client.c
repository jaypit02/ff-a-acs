/*
 * Copyright (c) 2021, Arm Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "test_database.h"

static uint32_t donate_lower_upper_boundary_32_client(uint32_t test_run_data)
{
    ffa_args_t payload;
    uint32_t status = VAL_SUCCESS;
    uint32_t client_logical_id = GET_CLIENT_LOGIC_ID(test_run_data);
    uint32_t server_logical_id = GET_SERVER_LOGIC_ID(test_run_data);
    ffa_endpoint_id_t sender = val_get_endpoint_id(client_logical_id);
    ffa_endpoint_id_t recipient = val_get_endpoint_id(server_logical_id);
    mb_buf_t mb;
    uint8_t *pages = NULL;
    uint64_t size = 0x1000;
    ffa_memory_handle_t handle;
    uint32_t msg_size;
    mem_region_init_t mem_region_init;
    struct ffa_memory_region_constituent constituents[2];
    const uint32_t constituents_count = sizeof(constituents) /
                sizeof(struct ffa_memory_region_constituent);

    if (val_is_ffa_feature_supported(FFA_MEM_DONATE_32))
    {
        LOG(TEST, "\tFFA_MEM_DONATE_32 not supported, skipping the check\n", 0, 0);
        return VAL_SKIP_CHECK;
    }

    mb.send = val_memory_alloc(size);
    mb.recv = val_memory_alloc(size);
    if (mb.send == NULL || mb.recv == NULL)
    {
        LOG(ERROR, "\tFailed to allocate RxTx buffer\n", 0, 0);
        status = VAL_ERROR_POINT(1);
        goto free_memory;
    }

    /* Map TX and RX buffers */
    if (val_rxtx_map_64((uint64_t)mb.send, (uint64_t)mb.recv, (uint32_t)(size/PAGE_SIZE_4K)))
    {
        LOG(ERROR, "\tRxTx Map failed\n", 0, 0);
        status = VAL_ERROR_POINT(2);
        goto free_memory;
    }

    /* Allocate 8KB page */
    pages = (uint8_t *)val_memory_alloc(size * 2);
    if (!pages)
    {
        LOG(ERROR, "\tMemory allocation failed\n", 0, 0);
        status = VAL_ERROR_POINT(3);
        goto rxtx_unmap;
    }

    val_select_server_fn_direct(test_run_data, 0, 0, 0, 0);

    constituents[0].address = val_mem_virt_to_phys((void *)pages);
    constituents[0].page_count = 1;
    constituents[1].address = val_mem_virt_to_phys((void *)pages + PAGE_SIZE_4K * 1);
    constituents[1].page_count = 1;

    mem_region_init.memory_region = mb.send;
    mem_region_init.sender = sender;
    mem_region_init.receiver = recipient;
    mem_region_init.tag = 0;
    mem_region_init.flags = 0;
    mem_region_init.data_access = FFA_DATA_ACCESS_NOT_SPECIFIED;
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
    mem_region_init.type = FFA_MEMORY_NOT_SPECIFIED_MEM;
    mem_region_init.cacheability = FFA_MEMORY_CACHE_WRITE_BACK;
    mem_region_init.shareability = FFA_MEMORY_OUTER_SHAREABLE;
    mem_region_init.multi_share = false;

    val_ffa_memory_region_init(&mem_region_init, constituents, constituents_count);
    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = mem_region_init.total_length;
    payload.arg2 = mem_region_init.fragment_length;
    val_ffa_mem_donate_32(&payload);
    if (payload.fid == FFA_ERROR_32)
    {
        LOG(ERROR, "\tMem_donate request failed err %x\n", payload.arg2, 0);
        status = VAL_ERROR_POINT(4);
        goto rxtx_unmap;
    }

    handle = ffa_mem_success_handle(payload);

    /* Pass memory handle to the server using direct message */
    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 =  ((uint32_t)sender << 16) | recipient;
    payload.arg3 =  handle;
    val_ffa_msg_send_direct_req_64(&payload);
    if (payload.fid == FFA_ERROR_32)
    {
        LOG(ERROR, "\tDirect request failed err %x\n", payload.arg2, 0);
        status = VAL_ERROR_POINT(5);
        goto rxtx_unmap;
    }

    /* Regain the ownership back from the server */
    handle = payload.arg3;
    mem_region_init.memory_region = mb.send;
    mem_region_init.sender = recipient;
    mem_region_init.receiver = sender;
    mem_region_init.tag = 0;
    mem_region_init.flags = 0;
    mem_region_init.data_access = FFA_DATA_ACCESS_RW;
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NX;
    mem_region_init.type = FFA_MEMORY_NORMAL_MEM;
    mem_region_init.cacheability = FFA_MEMORY_CACHE_WRITE_BACK;
    mem_region_init.shareability = FFA_MEMORY_OUTER_SHAREABLE;
    msg_size = val_ffa_memory_retrieve_request_init(&mem_region_init, handle);

    val_memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = msg_size;
    payload.arg2 = msg_size;
    val_ffa_mem_retrieve_32(&payload);
    if (payload.fid != FFA_MEM_RETRIEVE_RESP_32)
    {
        LOG(ERROR, "\tMem retrieve request failed err %x\n", payload.arg2, 0);
        status =  VAL_ERROR_POINT(6);
        goto rxtx_unmap;
    }

    val_select_server_fn_direct(test_run_data, 0, 0, 0, 0);

    if (val_rx_release())
    {
        LOG(ERROR, "\tval_rx_release failed\n", 0, 0);
        status = status ? status : VAL_ERROR_POINT(8);
    }
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

    if (val_memory_free(pages, size * 2))
    {
        LOG(ERROR, "\tval_mem_free failed\n", 0, 0);
        status = status ? status : VAL_ERROR_POINT(11);
    }

    payload = val_select_server_fn_direct(test_run_data, 0, 0, 0, 0);

    return status ? status : (uint32_t)payload.arg3;
}

uint32_t donate_lower_upper_boundary_32_spsp_client(uint32_t test_run_data)
{
    return donate_lower_upper_boundary_32_client(test_run_data);
}

uint32_t donate_lower_upper_boundary_32_vmvm_client(uint32_t test_run_data)
{
    return donate_lower_upper_boundary_32_client(test_run_data);
}
