// SPDX-FileCopyrightText: 2022 - 2023 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

#include "_gaussian_kernel.hpp"
#include <CL/sycl.hpp>
#include <dpctl4pybind11.hpp>

template <typename... Args> bool ensure_compatibility(const Args &...args)
{
    std::vector<dpctl::tensor::usm_ndarray> arrays = {args...};

    auto arr = arrays.at(0);
    auto q = arr.get_queue();
    auto type_flag = arr.get_typenum();
    auto arr_size = arr.get_size();

    for (auto &arr : arrays) {
        if (!(arr.get_flags() & (USM_ARRAY_C_CONTIGUOUS))) {
            std::cerr << "All arrays need to be C contiguous.\n";
            return false;
        }
        if (arr.get_typenum() != type_flag) {
            std::cerr << "All arrays should be of same elemental type.\n";
            return false;
        }
        if (arr.get_ndim() > 1) {
            std::cerr << "All arrays expected to be single-dimensional.\n";
            return false;
        }
    }
    return true;
}

void gaussian_sync(dpctl::tensor::usm_ndarray a,
                   dpctl::tensor::usm_ndarray b,
                   dpctl::tensor::usm_ndarray m,
                   int size,
                   int block_sizeXY,
                   dpctl::tensor::usm_ndarray result)
{
    if (!ensure_compatibility(a, m, b, result))
        throw std::runtime_error("Input arrays are not acceptable.");

    int t;

    sycl::queue q_ct1;

    int block_size, grid_size;
    block_size = q_ct1.get_device()
                     .get_info<cl::sycl::info::device::max_work_group_size>();
    grid_size = (size / block_size) + (!(size % block_size) ? 0 : 1);

    sycl::range<3> dimBlock(1, 1, block_size);
    sycl::range<3> dimGrid(1, 1, grid_size);

    int blocksize2d, gridsize2d;
    blocksize2d = block_sizeXY;
    gridsize2d = (size / blocksize2d) + (!(size % blocksize2d ? 0 : 1));

    sycl::range<3> dimBlockXY(1, blocksize2d, blocksize2d);
    sycl::range<3> dimGridXY(1, gridsize2d, gridsize2d);

    auto a_value = a.get_data<double>();
    auto b_value = b.get_data<double>();
    auto m_value = m.get_data<double>();

    for (t = 0; t < (size - 1); t++) {
        /*
        DPCT1049:7: The workgroup size passed to the SYCL kernel may
        exceed the limit. To get the device limit, query
        info::device::max_work_group_size. Adjust the workgroup size if
        needed.
        */
        q_ct1.submit([&](sycl::handler &cgh) {
            auto size_ct2 = size;
            cgh.parallel_for(sycl::nd_range<3>(dimGrid * dimBlock, dimBlock),
                             [=](sycl::nd_item<3> item_ct1) {
                                 gaussian_kernel_1(m_value, a_value, size_ct2,
                                                   t, item_ct1);
                             });
        });
        q_ct1.wait_and_throw();
        /*
        DPCT1049:8: The workgroup size passed to the SYCL kernel may
        exceed the limit. To get the device limit, query
        info::device::max_work_group_size. Adjust the workgroup size if
        needed.
        */
        q_ct1.submit([&](sycl::handler &cgh) {
            auto size_ct3 = size;
            auto size_t_ct4 = size - t;

            cgh.parallel_for(
                sycl::nd_range<3>(dimGridXY * dimBlockXY, dimBlockXY),
                [=](sycl::nd_item<3> item_ct1) {
                    gaussian_kernel_2(m_value, a_value, b_value, size_ct3,
                                      size_t_ct4, t, item_ct1);
                });
        });
        q_ct1.wait_and_throw();
    }
    // Copying the final answer
    auto result_value = result.get_data<double>();

    for (int i = 0; i < size; i++) {

        result_value[size - i - 1] = b_value[size - i - 1];

        for (int j = 0; j < i; j++) {
            result_value[size - i - 1] -=
                *(a_value + size * (size - i - 1) + (size - j - 1)) *
                result_value[size - j - 1];
        }

        result_value[size - i - 1] =
            result_value[size - i - 1] /
            *(a_value + size * (size - i - 1) + (size - i - 1));
    }
}

PYBIND11_MODULE(_gaussian_sycl, m)
{
    // Import the dpctl extensions
    import_dpctl();

    m.def("gaussian", &gaussian_sync,
          "DPC++ implementation of the gaussian elimination", py::arg("a"),
          py::arg("b"), py::arg("m"), py::arg("size"), py::arg("block_sizeXY"),
          py::arg("result"));
}
