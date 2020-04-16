#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "sps_pps_parser.h"
#ifdef __cplusplus 
extern "C" {
#endif 

#define SPS_PPS_DEBUG
#undef  SPS_PPS_DEBUG

#define MAX_LEN 32

    static int get_1bit(void *h) {
        get_bit_context *ptr = (get_bit_context *) h;
        int ret = 0;
        uint8_t *cur_char = NULL;
        uint8_t shift;

        if (NULL == ptr) {
            ret = -1;
            goto exit;
        }

        cur_char = ptr->buf + (ptr->bit_pos >> 3);
        shift = 7 - (ptr->cur_bit_pos);
        ptr->bit_pos++;
        ptr->cur_bit_pos = ptr->bit_pos & 0x7;
        ret = ((*cur_char) >> shift) & 0x01;

exit:
        return ret;
    }

    /**
     *  @brief Function get_bits()  讀n個bits，n不能超過32
     *  @param[in]     h     get_bit_context structrue
     *  @param[in]     n     how many bits you want?
     *  @retval        0: success, -1 : failure
     *  @pre
     *  @post
     */
    static int get_bits(void *h, int n) {
        get_bit_context *ptr = (get_bit_context *) h;
        uint8_t temp[5] = {0};
        uint8_t *cur_char = NULL;
        uint8_t nbyte;
        uint8_t shift;
        uint32_t result = 0;
        uint64_t ret = 0;

        if (NULL == ptr) {
            ret = -1;
            goto exit;
        }

        if (n > MAX_LEN) {
            n = MAX_LEN;
        }
        if ((ptr->bit_pos + n) > ptr->total_bit) {
            n = ptr->total_bit - ptr->bit_pos;
        }

        cur_char = ptr->buf + (ptr->bit_pos >> 3);
        nbyte = (ptr->cur_bit_pos + n + 7) >> 3;
        shift = (8 - (ptr->cur_bit_pos + n)) & 0x07;

        memcpy(&temp[5 - nbyte], cur_char, nbyte);
        ret = (uint32_t) temp[0] << 24;
        ret = ret << 8;
        ret = ((uint32_t) temp[1] << 24) | ((uint32_t) temp[2] << 16)\
		| ((uint32_t) temp[3] << 8) | temp[4];

        ret = (ret >> shift) & (((uint64_t) 1 << n) - 1);

        result = ret;
        ptr->bit_pos += n;
        ptr->cur_bit_pos = ptr->bit_pos & 0x7;

exit:
        return result;
    }

    static int parse_codenum(void *buf) {
        uint8_t leading_zero_bits = -1;
        uint8_t b;
        uint32_t code_num = 0;

        for (b = 0; !b; leading_zero_bits++) {
            b = get_1bit(buf);
        }

        code_num = ((uint32_t) 1 << leading_zero_bits) - 1 + get_bits(buf, leading_zero_bits);

        return code_num;
    }

    static int parse_ue(void *buf) {
        return parse_codenum(buf);
    }

    static int parse_se(void *buf) {
        int ret = 0;
        int code_num;

        code_num = parse_codenum(buf);
        ret = (code_num + 1) >> 1;
        ret = (code_num & 0x01) ? ret : -ret;

        return ret;
    }

    static void get_bit_context_free(void *buf) {
        get_bit_context *ptr = (get_bit_context *) buf;

        if (ptr) {
            if (ptr->buf) {
                free(ptr->buf);
            }

            free(ptr);
        }
    }

    static void *de_emulation_prevention(void *buf) {
        get_bit_context *ptr = NULL;
        get_bit_context *buf_ptr = (get_bit_context *) buf;
        int i = 0, j = 0;
        uint8_t *tmp_ptr = NULL;
        int tmp_buf_size = 0;
        int val = 0;
        if (NULL == buf_ptr) {
            goto exit;
        }
        ptr = (get_bit_context *) malloc(sizeof (get_bit_context));
        if (NULL == ptr) {
            goto exit;
        }
        memcpy(ptr, buf_ptr, sizeof (get_bit_context));
        // printf("fun = %s line = %d ptr->buf_size=%d \n", __FUNCTION__, __LINE__, ptr->buf_size);
        ptr->buf = (uint8_t *) malloc(ptr->buf_size);
        if (NULL == ptr->buf) {
            goto exit;
        }
        memcpy(ptr->buf, buf_ptr->buf, buf_ptr->buf_size);
        tmp_ptr = ptr->buf;
        tmp_buf_size = ptr->buf_size;
        for (i = 0; i < (tmp_buf_size - 2); i++) {
            val = (tmp_ptr[i] ^ 0x00) + (tmp_ptr[i + 1] ^ 0x00) + (tmp_ptr[i + 2] ^ 0x03);
            if (val == 0) {
                for (j = i + 2; j < tmp_buf_size - 1; j++) {
                    tmp_ptr[j] = tmp_ptr[j + 1];
                }
                ptr->buf_size--;
            }
        }

        ptr->total_bit = ptr->buf_size << 3;
        return (void *) ptr;

exit:
        get_bit_context_free(ptr);
        return NULL;
    }

    static int vui_parameters_set(void *buf, vui_parameters_t *vui_ptr) {
        int ret = 0;
        int SchedSelIdx = 0;

        if (NULL == vui_ptr || NULL == buf) {
            ret = -1;
            goto exit;
        }

        vui_ptr->aspect_ratio_info_present_flag = get_1bit(buf);
        if (vui_ptr->aspect_ratio_info_present_flag) {
            vui_ptr->aspect_ratio_idc = get_bits(buf, 8);
            if (vui_ptr->aspect_ratio_idc == Extended_SAR) {
                vui_ptr->sar_width = get_bits(buf, 16);
                vui_ptr->sar_height = get_bits(buf, 16);
            }
        }

        vui_ptr->overscan_info_present_flag = get_1bit(buf);
        if (vui_ptr->overscan_info_present_flag) {
            vui_ptr->overscan_appropriate_flag = get_1bit(buf);
        }

        vui_ptr->video_signal_type_present_flag = get_1bit(buf);
        if (vui_ptr->video_signal_type_present_flag) {
            vui_ptr->video_format = get_bits(buf, 3);
            vui_ptr->video_full_range_flag = get_1bit(buf);

            vui_ptr->colour_description_present_flag = get_1bit(buf);
            if (vui_ptr->colour_description_present_flag) {
                vui_ptr->colour_primaries = get_bits(buf, 8);
                vui_ptr->transfer_characteristics = get_bits(buf, 8);
                vui_ptr->matrix_coefficients = get_bits(buf, 8);
            }
        }

        vui_ptr->chroma_loc_info_present_flag = get_1bit(buf);
        if (vui_ptr->chroma_loc_info_present_flag) {
            vui_ptr->chroma_sample_loc_type_top_field = parse_ue(buf);
            vui_ptr->chroma_sample_loc_type_bottom_field = parse_ue(buf);
        }

        vui_ptr->timing_info_present_flag = get_1bit(buf);
        if (vui_ptr->timing_info_present_flag) {
            vui_ptr->num_units_in_tick = get_bits(buf, 32);
            vui_ptr->time_scale = get_bits(buf, 32);
            vui_ptr->fixed_frame_rate_flag = get_1bit(buf);
        }

        vui_ptr->nal_hrd_parameters_present_flag = get_1bit(buf);
        if (vui_ptr->nal_hrd_parameters_present_flag) {
            vui_ptr->cpb_cnt_minus1 = parse_ue(buf);
            vui_ptr->bit_rate_scale = get_bits(buf, 4);
            vui_ptr->cpb_size_scale = get_bits(buf, 4);

            for (SchedSelIdx = 0; SchedSelIdx <= vui_ptr->cpb_cnt_minus1; SchedSelIdx++) {
                vui_ptr->bit_rate_value_minus1[SchedSelIdx] = parse_ue(buf);
                vui_ptr->cpb_size_value_minus1[SchedSelIdx] = parse_ue(buf);
                vui_ptr->cbr_flag[SchedSelIdx] = get_1bit(buf);
            }

            vui_ptr->initial_cpb_removal_delay_length_minus1 = get_bits(buf, 5);
            vui_ptr->cpb_removal_delay_length_minus1 = get_bits(buf, 5);
            vui_ptr->dpb_output_delay_length_minus1 = get_bits(buf, 5);
            vui_ptr->time_offset_length = get_bits(buf, 5);
        }


        vui_ptr->vcl_hrd_parameters_present_flag = get_1bit(buf);
        if (vui_ptr->vcl_hrd_parameters_present_flag) {
            vui_ptr->cpb_cnt_minus1 = parse_ue(buf);
            vui_ptr->bit_rate_scale = get_bits(buf, 4);
            vui_ptr->cpb_size_scale = get_bits(buf, 4);

            for (SchedSelIdx = 0; SchedSelIdx <= vui_ptr->cpb_cnt_minus1; SchedSelIdx++) {
                vui_ptr->bit_rate_value_minus1[SchedSelIdx] = parse_ue(buf);
                vui_ptr->cpb_size_value_minus1[SchedSelIdx] = parse_ue(buf);
                vui_ptr->cbr_flag[SchedSelIdx] = get_1bit(buf);
            }
            vui_ptr->initial_cpb_removal_delay_length_minus1 = get_bits(buf, 5);
            vui_ptr->cpb_removal_delay_length_minus1 = get_bits(buf, 5);
            vui_ptr->dpb_output_delay_length_minus1 = get_bits(buf, 5);
            vui_ptr->time_offset_length = get_bits(buf, 5);
        }

        if (vui_ptr->nal_hrd_parameters_present_flag \
		|| vui_ptr->vcl_hrd_parameters_present_flag) {
            vui_ptr->low_delay_hrd_flag = get_1bit(buf);
        }

        vui_ptr->pic_struct_present_flag = get_1bit(buf);

        vui_ptr->bitstream_restriction_flag = get_1bit(buf);
        if (vui_ptr->bitstream_restriction_flag) {
            vui_ptr->motion_vectors_over_pic_boundaries_flag = get_1bit(buf);
            vui_ptr->max_bytes_per_pic_denom = parse_ue(buf);
            vui_ptr->max_bits_per_mb_denom = parse_ue(buf);
            vui_ptr->log2_max_mv_length_horizontal = parse_ue(buf);
            vui_ptr->log2_max_mv_length_vertical = parse_ue(buf);
            vui_ptr->num_reorder_frames = parse_ue(buf);
            vui_ptr->max_dec_frame_buffering = parse_ue(buf);
        }

exit:
        return ret;
    }

    int h264dec_seq_parameter_set(void *buf_ptr, SPS *sps_ptr) {
        SPS *sps = sps_ptr;
        int ret = 0;
        int profile_idc = 0;
        int i, j, last_scale, next_scale, delta_scale;
        void *buf = NULL;

        if (NULL == buf_ptr || NULL == sps) {
            ret = -1;
            goto exit;
        }

        memset((void *) sps, 0, sizeof (SPS));
        buf = de_emulation_prevention(buf_ptr);
        if (NULL == buf) {
            ret = -1;
            goto exit;
        }
        sps->profile_idc = get_bits(buf, 8);
        sps->constraint_set0_flag = get_1bit(buf);
        sps->constraint_set1_flag = get_1bit(buf);
        sps->constraint_set2_flag = get_1bit(buf);
        sps->constraint_set3_flag = get_1bit(buf);
        sps->reserved_zero_4bits = get_bits(buf, 4);
        sps->level_idc = get_bits(buf, 8);
        sps->seq_parameter_set_id = parse_ue(buf);
        profile_idc = sps->profile_idc;
        if ((profile_idc == 100) || (profile_idc == 110) || (profile_idc == 122) || (profile_idc == 244)
                || (profile_idc == 44) || (profile_idc == 83) || (profile_idc == 86) || (profile_idc == 118) || \
		(profile_idc == 128)) {
            sps->chroma_format_idc = parse_ue(buf);
            if (sps->chroma_format_idc == 3) {
                sps->separate_colour_plane_flag = get_1bit(buf);
            }

            sps->bit_depth_luma_minus8 = parse_ue(buf);
            sps->bit_depth_chroma_minus8 = parse_ue(buf);
            sps->qpprime_y_zero_transform_bypass_flag = get_1bit(buf);
            sps->seq_scaling_matrix_present_flag = get_1bit(buf);
            if (sps->seq_scaling_matrix_present_flag) {
                for (i = 0; i < ((sps->chroma_format_idc != 3) ? 8 : 12); i++) {
                    sps->seq_scaling_list_present_flag[i] = get_1bit(buf);
                    if (sps->seq_scaling_list_present_flag[i]) {
                        if (i < 6) {
                            for (j = 0; j < 16; j++) {
                                last_scale = 8;
                                next_scale = 8;
                                if (next_scale != 0) {
                                    delta_scale = parse_se(buf);
                                    next_scale = (last_scale + delta_scale + 256) % 256;
                                    sps->UseDefaultScalingMatrix4x4Flag[i] = ((j == 0) && (next_scale == 0));
                                }
                                sps->ScalingList4x4[i][j] = (next_scale == 0) ? last_scale : next_scale;
                                last_scale = sps->ScalingList4x4[i][j];
                            }
                        } else {
                            int ii = i - 6;
                            next_scale = 8;
                            last_scale = 8;
                            for (j = 0; j < 64; j++) {
                                if (next_scale != 0) {
                                    delta_scale = parse_se(buf);
                                    next_scale = (last_scale + delta_scale + 256) % 256;
                                    sps->UseDefaultScalingMatrix8x8Flag[ii] = ((j == 0) && (next_scale == 0));
                                }
                                sps->ScalingList8x8[ii][j] = (next_scale == 0) ? last_scale : next_scale;
                                last_scale = sps->ScalingList8x8[ii][j];
                            }
                        }
                    }
                }
            }
        }
        sps->log2_max_frame_num_minus4 = parse_ue(buf);
        sps->pic_order_cnt_type = parse_ue(buf);
        if (sps->pic_order_cnt_type == 0) {
            sps->log2_max_pic_order_cnt_lsb_minus4 = parse_ue(buf);
        } else if (sps->pic_order_cnt_type == 1) {
            sps->delta_pic_order_always_zero_flag = get_1bit(buf);
            sps->offset_for_non_ref_pic = parse_se(buf);
            sps->offset_for_top_to_bottom_field = parse_se(buf);

            sps->num_ref_frames_in_pic_order_cnt_cycle = parse_ue(buf);
            for (i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++) {
                sps->offset_for_ref_frame_array[i] = parse_se(buf);
            }
        }
        sps->num_ref_frames = parse_ue(buf);
        sps->gaps_in_frame_num_value_allowed_flag = get_1bit(buf);
        sps->pic_width_in_mbs_minus1 = parse_ue(buf);
        sps->pic_height_in_map_units_minus1 = parse_ue(buf);
        sps->frame_mbs_only_flag = get_1bit(buf);
        if (!sps->frame_mbs_only_flag) {
            sps->mb_adaptive_frame_field_flag = get_1bit(buf);
        }
        sps->direct_8x8_inference_flag = get_1bit(buf);

        sps->frame_cropping_flag = get_1bit(buf);
        if (sps->frame_cropping_flag) {
            sps->frame_crop_left_offset = parse_ue(buf);
            sps->frame_crop_right_offset = parse_ue(buf);
            sps->frame_crop_top_offset = parse_ue(buf);
            sps->frame_crop_bottom_offset = parse_ue(buf);
        }
        sps->vui_parameters_present_flag = get_1bit(buf);
        if (sps->vui_parameters_present_flag) {
            vui_parameters_set(buf, &sps->vui_parameters);
        }

exit:
        get_bit_context_free(buf);
        return ret;
    }

    static int more_rbsp_data(void *buf) {
        get_bit_context *ptr = (get_bit_context *) buf;
        get_bit_context tmp;

        if (NULL == buf) {
            return -1;
        }

        memset(&tmp, 0, sizeof (get_bit_context));
        memcpy(&tmp, ptr, sizeof (get_bit_context));

        for (tmp.bit_pos = ptr->total_bit - 1; tmp.bit_pos > ptr->bit_pos; tmp.bit_pos -= 2) {
            if (get_1bit(&tmp)) {
                break;
            }
        }
        return tmp.bit_pos == ptr->bit_pos ? 0 : 1;
    }

    int h264dec_picture_parameter_set(void *buf_ptr, PPS *pps_ptr) {
        PPS *pps = pps_ptr;
        int ret = 0;
        void *buf = NULL;
        int iGroup = 0;
        int i, j, last_scale, next_scale, delta_scale;

        if (NULL == buf_ptr || NULL == pps_ptr) {
            ret = -1;
            goto exit;
        }

        memset((void *) pps, 0, sizeof (PPS));

        buf = de_emulation_prevention(buf_ptr);
        if (NULL == buf) {
            ret = -1;
            goto exit;
        }

        pps->pic_parameter_set_id = parse_ue(buf);
        pps->seq_parameter_set_id = parse_ue(buf);
        pps->entropy_coding_mode_flag = get_1bit(buf);
        pps->pic_order_present_flag = get_1bit(buf);

        pps->num_slice_groups_minus1 = parse_ue(buf);
        if (pps->num_slice_groups_minus1 > 0) {
            pps->slice_group_map_type = parse_ue(buf);
            if (pps->slice_group_map_type == 0) {
                for (iGroup = 0; iGroup <= pps->num_slice_groups_minus1; iGroup++) {
                    pps->run_length_minus1[iGroup] = parse_ue(buf);
                }
            } else if (pps->slice_group_map_type == 2) {
                for (iGroup = 0; iGroup <= pps->num_slice_groups_minus1; iGroup++) {
                    pps->top_left[iGroup] = parse_ue(buf);
                    pps->bottom_right[iGroup] = parse_ue(buf);
                }
            } else if (pps->slice_group_map_type == 3 || pps->slice_group_map_type == 4 || pps->slice_group_map_type == 5) {
                pps->slice_group_change_direction_flag = get_1bit(buf);
                pps->slice_group_change_rate_minus1 = parse_ue(buf);
            } else if (pps->slice_group_map_type == 6) {
                pps->pic_size_in_map_units_minus1 = parse_ue(buf);
                for (i = 0; i < pps->pic_size_in_map_units_minus1; i++) {
                    pps->slice_group_id[i] = get_bits(buf, pps->pic_size_in_map_units_minus1);
                }
            }
        }

        pps->num_ref_idx_10_active_minus1 = parse_ue(buf);
        pps->num_ref_idx_11_active_minus1 = parse_ue(buf);
        pps->weighted_pred_flag = get_1bit(buf);
        pps->weighted_bipred_idc = get_bits(buf, 2);
        pps->pic_init_qp_minus26 = parse_se(buf); /*relative26*/
        pps->pic_init_qs_minus26 = parse_se(buf); /*relative26*/
        pps->chroma_qp_index_offset = parse_se(buf);
        pps->deblocking_filter_control_present_flag = get_1bit(buf);
        pps->constrained_intra_pred_flag = get_1bit(buf);
        pps->redundant_pic_cnt_present_flag = get_1bit(buf);

        if (more_rbsp_data(buf)) {
            pps->transform_8x8_mode_flag = get_1bit(buf);
            pps->pic_scaling_matrix_present_flag = get_1bit(buf);
            if (pps->pic_scaling_matrix_present_flag) {
                for (i = 0; i < 6 + 2 * pps->transform_8x8_mode_flag; i++) {
                    pps->pic_scaling_list_present_flag[i] = get_1bit(buf);
                    if (pps->pic_scaling_list_present_flag[i]) {
                        if (i < 6) {
                            for (j = 0; j < 16; j++) {
                                next_scale = 8;
                                last_scale = 8;
                                if (next_scale != 0) {
                                    delta_scale = parse_se(buf);
                                    next_scale = (last_scale + delta_scale + 256) % 256;
                                    pps->UseDefaultScalingMatrix4x4Flag[i] = ((j == 0) && (next_scale == 0));
                                }
                                pps->ScalingList4x4[i][j] = (next_scale == 0) ? last_scale : next_scale;
                                last_scale = pps->ScalingList4x4[i][j];
                            }
                        } else {
                            int ii = i - 6;
                            next_scale = 8;
                            last_scale = 8;
                            for (j = 0; j < 64; j++) {
                                if (next_scale != 0) {
                                    delta_scale = parse_se(buf);
                                    next_scale = (last_scale + delta_scale + 256) % 256;
                                    pps->UseDefaultScalingMatrix8x8Flag[ii] = ((j == 0) && (next_scale == 0));
                                }
                                pps->ScalingList8x8[ii][j] = (next_scale == 0) ? last_scale : next_scale;
                                last_scale = pps->ScalingList8x8[ii][j];
                            }
                        }
                    }
                }

                pps->second_chroma_qp_index_offset = parse_se(buf);
            }
        }

exit:
        get_bit_context_free(buf);
        return ret;
    }

    int h264_get_width(SPS *sps_ptr) {
        return (sps_ptr->pic_width_in_mbs_minus1 + 1) * 16;
    }

    int h264_get_height(SPS *sps_ptr) {
        // printf("fun = %s line = %d sps_ptr->frame_mbs_only_flag=%d \n", __FUNCTION__, __LINE__, sps_ptr->frame_mbs_only_flag);
        return (sps_ptr->pic_height_in_map_units_minus1 + 1) * 16 * (2 - sps_ptr->frame_mbs_only_flag);
    }

    int h264_get_format(SPS *sps_ptr) {
        return sps_ptr->frame_mbs_only_flag;
    }

    int h264_get_framerate(float *framerate, SPS *sps_ptr) {
        if (sps_ptr->vui_parameters.timing_info_present_flag) {
            if (sps_ptr->frame_mbs_only_flag) {
                //*framerate = (float)sps_ptr->vui_parameters.time_scale / (float)sps_ptr->vui_parameters.num_units_in_tick;
                *framerate = (float) sps_ptr->vui_parameters.time_scale / (float) sps_ptr->vui_parameters.num_units_in_tick / 2.0;
                //fr_int = sps_ptr->vui_parameters.time_scale / sps_ptr->vui_parameters.num_units_in_tick;
            } else {
                *framerate = (float) sps_ptr->vui_parameters.time_scale / (float) sps_ptr->vui_parameters.num_units_in_tick / 2.0;
                //fr_int = sps_ptr->vui_parameters.time_scale / sps_ptr->vui_parameters.num_units_in_tick / 2;
            }
            return 0;
        } else {
            return 1;
        }
    }

    void memcpy_sps_data(uint8_t *dst, uint8_t *src, int len) {
        int tmp;
        for (tmp = 0; tmp < len; tmp++) {
            //printf("0x%02x ", src[tmp]);
            dst[(tmp / 4) * 4 + (3 - (tmp % 4))] = src[tmp];
        }
    }
    /*_*/
#ifdef __cplusplus 
}
#endif
