/*
 * Copyright (C) 2014-2015, 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 */

#include <CL/sycl.hpp>
#include <mathimf.h>
#include "constants_header.h"

using namespace cl::sycl;

#ifdef __DO_FLOAT__
#   define SQRT(x)     sqrtf(x)
#else
#   define SQRT(x)     sqrt(x)
#endif

void pairwise_distance( queue* q, size_t nopt, struct point * p1, struct point * p2, tfloat* distance_op ) {
  // allocate GPU data using malloc_device
  struct point* d_p1, *d_p2;
  tfloat* d_distance_op;
  d_p1 = (struct point*)malloc_shared( nopt * sizeof(struct point), *q);
  d_p2 = (struct point*)malloc_shared( nopt * sizeof(struct point), *q);
  d_distance_op = (tfloat*)malloc_shared( nopt * nopt * sizeof(tfloat), *q);

  // copy data host to device
  q->memcpy(d_p1, p1, nopt * sizeof(struct point));
  q->memcpy(d_p2, p2, nopt * sizeof(struct point));

  q->wait();

  q->submit([&](handler& h) {
      h.parallel_for<class theKernel>(range<1>{nopt}, [=](id<1> myID) {
	  size_t i = myID[0];
	  for (size_t j = 0; j < nopt; j++) {
	    tfloat tmp = d_p1[i].x - d_p2[j].x;
	    tfloat d = tmp * tmp;

	    tmp = d_p1[i].y - d_p2[j].y;
	    d += tmp * tmp;

	    tmp = d_p1[i].z - d_p2[j].z;
	    d += tmp * tmp;

	    if (d != 0.0) {
	      d_distance_op[i*nopt + j] = sqrt(d);
	    }
	  }
	});
    });

  q->wait();

  q->memcpy(distance_op, d_distance_op, nopt * nopt * sizeof(tfloat));

  q->wait();

  free(d_p1,q->get_context());
  free(d_p2,q->get_context());
  free(d_distance_op,q->get_context());
}
