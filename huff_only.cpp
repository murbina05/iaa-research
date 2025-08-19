/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

//* [QPL_LOW_LEVEL_CANNED_MODE_EXAMPLE] */

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "qpl/qpl.h"

/**
 * @brief This example requires a command line argument to set the execution path. Valid values are `software_path`
 * and `hardware_path`. This example also requires a second command line argument which specifies the dataset path.
 * In QPL, @ref qpl_path_software (`Software Path`) means that computations will be done with CPU.
 * Accelerator can be used instead of CPU. In this case, @ref qpl_path_hardware (`Hardware Path`) must be specified.
 * If there is no difference where calculations should be done, @ref qpl_path_auto (`Auto Path`) can be used to allow
 * the library to chose the path to execute. The Auto Path usage is not demonstrated by this example.
 *
 * @warning ---! Important !---
 * `Hardware Path` doesn't support all features declared for `Software Path`
 *
 */

auto main(int argc, char** argv) -> int {
    std::cout << "Intel(R) Query Processing Library version is " << qpl_get_library_version() << ".\n";

    qpl_path_t execution_path = qpl_path_software;

    // Get path from input argument
    // const int extra_arg = 1;
    // const int parse_ret = parse_execution_path(argc, argv, &execution_path, extra_arg);
    // if (parse_ret) { return 1; }

    const std::string dataset_path = argv[2];
    // const std::string table_path = argv[3];

    // std::ifstream infile(table_path, std::ios::binary);

    qpl_huffman_table_t huffman_table = nullptr;
    qpl_status status;

    status = qpl_huffman_only_table_create(compression_table_type, execution_path, DEFAULT_ALLOCATOR_C,
                                           &huffman_table);

    for (const auto& path : std::filesystem::directory_iterator(dataset_path)) {
        std::ifstream file(path.path().string(), std::ifstream::binary);

        if (!file.is_open()) {
            std::cout << "Unable to open file in " << dataset_path << '\n';
            return 1;
        }

        std::vector<uint8_t> source;
        std::vector<uint8_t> destination;
        std::vector<uint8_t> reference;

        source.reserve(path.file_size());
        source.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

        // Get compression buffer size estimate
        const uint32_t compression_size = qpl_get_safe_deflate_compression_buffer_size(source.size());
        if (compression_size == 0) {
            std::cout << "Invalid source size. Source size exceeds the maximum supported size.\n";
            return 1;
        }

        destination.resize(compression_size);
        reference.resize(source.size());

        std::unique_ptr<uint8_t[]> job_buffer;
        uint32_t                   size = 0;
        qpl_histogram              deflate_histogram {};

        // Job initialization
        qpl_status status = qpl_get_job_size(execution_path, &size);
        if (status != QPL_STS_OK) {
            std::cout << "An error " << status << " acquired during job size getting.\n";
            return 1;
        }

        job_buffer   = std::make_unique<uint8_t[]>(size);
        qpl_job* job = reinterpret_cast<qpl_job*>(job_buffer.get());

        status = qpl_init_job(execution_path, job);
        if (status != QPL_STS_OK) {
            std::cout << "An error " << status << " acquired during job initializing.\n";
            return 1;
        }

        

        // Now perform canned mode compression
        job->op            = qpl_op_compress;
        job->level         = qpl_default_level;
        job->next_in_ptr   = source.data();
        job->next_out_ptr  = destination.data();
        job->available_in  = static_cast<uint32_t>(source.size());
        job->available_out = static_cast<uint32_t>(destination.size());
        job->flags         = QPL_FLAG_FIRST | QPL_FLAG_LAST | QPL_FLAG_NO_HDRS | QPL_FLAG_GEN_LITERALS |
                          QPL_FLAG_DYNAMIC_HUFFMAN | QPL_FLAG_OMIT_VERIFY;
        job->huffman_table = huffman_table;

        // Compression
        status = qpl_execute_job(job);
        if (status != QPL_STS_OK) {
            std::cout << "An error " << status << " acquired during compression.\n";
            qpl_huffman_table_destroy(huffman_table);
            return 1;
        }
        // uint32_t* serlialized_buff;
        // size_t serialized_size = 0U;
        // // serialization_options_t format = serialization_raw;  // or serialization_raw

        // status = qpl_huffman_table_get_serialized_size(huffman_table, DEFAULT_SERIALIZATION_OPTIONS, &serialized_size);
        // if (status != QPL_STS_OK) {
        //    std::cout << "An error " << status << " acquired during getting serialized size of Huffman table.\n";
        //     qpl_huffman_table_destroy(huffman_table);
        //     return 1;
        // }
        // const std::unique_ptr<uint8_t[]> unique_buffer = std::make_unique<uint8_t[]>(serialized_size);
        // uint8_t*                         buffer        = reinterpret_cast<uint8_t*>(unique_buffer.get());


        //     // Serialization of a table
        // status = qpl_huffman_table_serialize(huffman_table, buffer, serialized_size, DEFAULT_SERIALIZATION_OPTIONS);
        // if (status != QPL_STS_OK) {
        //     std::cout << "An error " << status << " acquired during serializing Huffman table.\n";
        //     qpl_huffman_table_destroy(huffman_table);
        //     return 1;
        // }

        // std::ofstream outfile("huffman_only_table_S.bin", std::ios::binary);
        // outfile.write(reinterpret_cast<const char*>(buffer), serialized_size);
        // outfile.close();

        qpl_huffman_table_t d_huffman_table = NULL;
        status = qpl_huffman_only_table_create(decompression_table_type, execution_path, DEFAULT_ALLOCATOR_C,
                                           &d_huffman_table);
        const uint32_t last_bit_offset = job->last_bit_offset;
        const uint32_t compressed_size = job->total_out;

        // Initializing decompression table with the values from compression table
        status = qpl_huffman_table_init_with_other(d_huffman_table, huffman_table);
        if (status != QPL_STS_OK) {
            printf("An error acquired during decompression Huffman table initialization failed. Error status = %d\n",
                status);

            qpl_huffman_table_destroy(huffman_table);
            qpl_huffman_table_destroy(d_huffman_table);

            return status;
        }

        // Performing a decompression operation
        job->op            = qpl_op_decompress;
        job->next_in_ptr   = destination.data();
        job->next_out_ptr  = reference.data();
        job->available_in  = compressed_size;
        job->available_out = static_cast<uint32_t>(reference.size());
        job->ignore_end_bits = (8 - last_bit_offset) & 7;
        job->flags         = QPL_FLAG_NO_HDRS | QPL_FLAG_FIRST | QPL_FLAG_LAST;
        job->huffman_table = d_huffman_table;

        // Decompression
        status = qpl_execute_job(job);
        if (status != QPL_STS_OK) {
            std::cout << "An error " << status << " acquired during decompression.\n";
            qpl_huffman_table_destroy(huffman_table);
            return 1;
        }

        // Huffman table serlization 
        /*
        uint32_t* serlialized_buff;
        size_t serialized_size = 0U;
        // serialization_options_t format = serialization_raw;  // or serialization_raw

        status = qpl_huffman_table_get_serialized_size(huffman_table, DEFAULT_SERIALIZATION_OPTIONS, &serialized_size);
        if (status != QPL_STS_OK) {
           std::cout << "An error " << status << " acquired during getting serialized size of Huffman table.\n";
            qpl_huffman_table_destroy(huffman_table);
            return 1;
        }
        const std::unique_ptr<uint8_t[]> unique_buffer = std::make_unique<uint8_t[]>(serialized_size);
        uint8_t*                         buffer        = reinterpret_cast<uint8_t*>(unique_buffer.get());


            // Serialization of a table
        status = qpl_huffman_table_serialize(huffman_table, buffer, serialized_size, DEFAULT_SERIALIZATION_OPTIONS);
        if (status != QPL_STS_OK) {
            std::cout << "An error " << status << " acquired during serializing Huffman table.\n";
            qpl_huffman_table_destroy(huffman_table);
            return 1;
        }

        std::ofstream outfile("huffman_table_S.bin", std::ios::binary);
        outfile.write(reinterpret_cast<const char*>(buffer), serialized_size);
        outfile.close();
        */


        status = qpl_fini_job(job);
        if (status != QPL_STS_OK) {
            std::cout << "An error " << status << " acquired during job finalization.\n";
            return 1;
        }

        // Compare reference functions
        for (size_t i = 0; i < source.size(); i++) {
            if (source[i] != reference[i]) {
                std::cout << "Content wasn't successfully compressed and decompressed.\n";
                return 1;
            }
        }

        std::cout << "Content of " << path.path().filename() << " was successfully compressed and decompressed.\n";
        std::cout << ""
                     "Input size: "
                  << source.size() << ", compressed size: " << compressed_size
                  << ", compression ratio: " << (float)source.size() / (float)compressed_size << ".\n";
    }
    // Freeing resources
    status = qpl_huffman_table_destroy(huffman_table);
    if (status != QPL_STS_OK) {
        std::cout << "An error " << status << " acquired during destroying Huffman table.\n";
        return 1;
    }
    return 0;
}

//* [QPL_LOW_LEVEL_CANNED_MODE_EXAMPLE] */
