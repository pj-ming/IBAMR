// ---------------------------------------------------------------------
//
// Copyright (c) 2019 - 2019 by the IBAMR developers
// All rights reserved.
//
// This file is part of IBAMR.
//
// IBAMR is free software and is distributed under the 3-clause BSD
// license. The full text of the license can be found in the file
// COPYRIGHT at the top level directory of IBAMR.
//
// ---------------------------------------------------------------------

#include "ibtk/JacobianCalculator.h"
#include "ibtk/libmesh_utilities.h"
#include "ibtk/namespaces.h"

#include "tbox/Utilities.h"

#include "libmesh/dense_matrix.h"
#include "libmesh/elem.h"
#include "libmesh/enum_elem_type.h"
#include "libmesh/enum_fe_family.h"
#include "libmesh/enum_order.h"
#include "libmesh/enum_quadrature_type.h"
#include "libmesh/type_vector.h"
#include <libmesh/libmesh_version.h>
#include <libmesh/point.h>
#include <libmesh/quadrature.h>

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <memory>

namespace IBTK
{
JacobianCalculator::JacobianCalculator(const JacobianCalculator::key_type quad_key) : d_quad_key(quad_key)
{
    const ElemType elem_type = std::get<0>(d_quad_key);
    const QuadratureType quad_type = std::get<1>(d_quad_key);
    const Order order = std::get<2>(d_quad_key);

    const int dim = get_dim(elem_type);

    std::unique_ptr<QBase> quad_rule = QBase::build(quad_type, dim, order);
    quad_rule->init(elem_type);
    d_quad_points = quad_rule->get_points();
    d_quad_weights = quad_rule->get_weights();
}

template <int dim, int spacedim>
Mapping<dim, spacedim>::Mapping(const typename Mapping<dim, spacedim>::key_type quad_key) : JacobianCalculator(quad_key)
{
    d_JxW.resize(this->d_quad_weights.size());
    d_contravariants.resize(this->d_quad_weights.size());
}

template <int dim, int spacedim>
LagrangeMapping<dim, spacedim>::LagrangeMapping(const typename LagrangeMapping<dim, spacedim>::key_type quad_key)
    : Mapping<dim, spacedim>(quad_key), d_n_nodes(get_n_nodes(std::get<0>(this->d_quad_key)))
{
#if LIBMESH_VERSION_LESS_THAN(1, 4, 0)
    TBOX_ASSERT(d_n_nodes <= 27);
#else
    TBOX_ASSERT(d_n_nodes <= libMesh::Elem::max_n_nodes);
#endif
    const libMesh::ElemType elem_type = std::get<0>(this->d_quad_key);

    typename decltype(d_dphi)::extent_gen extents;
    d_dphi.resize(extents[d_n_nodes][this->d_quad_points.size()]);

    for (unsigned int node_n = 0; node_n < d_n_nodes; ++node_n)
    {
        for (unsigned int q = 0; q < this->d_quad_points.size(); ++q)
        {
            for (unsigned int d = 0; d < dim; ++d)
                d_dphi[node_n][q][d] = libMesh::FE<dim, libMesh::LAGRANGE>::shape_deriv(
                    elem_type, get_default_order(elem_type), node_n, d, this->d_quad_points[q]);
        }
    }
}

template <int dim, int spacedim>
const typename Mapping<dim, spacedim>::MappingData&
LagrangeMapping<dim, spacedim>::get(const libMesh::Elem* elem)
{
    // static_assert(spacedim <= LIBMESH_DIM);
    TBOX_ASSERT(elem->type() == std::get<0>(this->d_quad_key));
    std::copy(this->d_quad_weights.begin(), this->d_quad_weights.end(), this->d_JxW.begin());

    // max_n_nodes is a constant defined by libMesh - currently 27
#if LIBMESH_VERSION_LESS_THAN(1, 4, 0)
    double xs[27][spacedim];
#else
    double xs[libMesh::Elem::max_n_nodes][spacedim];
#endif

    for (unsigned int i = 0; i < d_n_nodes; ++i)
    {
        const libMesh::Point p = elem->point(i);
        for (unsigned int j = 0; j < spacedim; ++j) xs[i][j] = p(j);
    }

    for (unsigned int q = 0; q < this->d_JxW.size(); ++q)
    {
        auto& contravariant = this->d_contravariants[q];
        contravariant.setZero();
        for (unsigned int node_n = 0; node_n < d_n_nodes; ++node_n)
        {
            for (unsigned int i = 0; i < spacedim; ++i)
            {
                for (unsigned int j = 0; j < dim; ++j)
                {
                    contravariant(i, j) += xs[node_n][i] * d_dphi[node_n][q][j];
                }
            }
        }

        double J = 0.0;
        if (dim == spacedim)
        {
            J = contravariant.determinant();
        }
        else
        {
            Eigen::Matrix<double, dim, dim> Jac = contravariant.transpose() * contravariant;
            J = std::sqrt(Jac.determinant());
        }
        TBOX_ASSERT(J > 0.0);
        this->d_JxW[q] *= J;
    }

    return this->d_values;
}

const typename Mapping<2, 2>::MappingData&
Tri3Mapping::get(const libMesh::Elem* elem)
{
    TBOX_ASSERT(elem->type() == std::get<0>(this->d_quad_key));
    std::copy(d_quad_weights.begin(), d_quad_weights.end(), this->d_JxW.begin());

    const libMesh::Point p0 = elem->point(0);
    const libMesh::Point p1 = elem->point(1);
    const libMesh::Point p2 = elem->point(2);

    Eigen::Matrix<double, 2, 2> contravariant;
    contravariant(0, 0) = p1(0) - p0(0);
    contravariant(0, 1) = p2(0) - p0(0);
    contravariant(1, 0) = p1(1) - p0(1);
    contravariant(1, 1) = p2(1) - p0(1);
    std::fill(this->d_contravariants.begin(), this->d_contravariants.end(), contravariant);

    const double J = contravariant.determinant();
    TBOX_ASSERT(J > 0.0);
    for (double& jxw : this->d_JxW) jxw *= J;

    return this->d_values;
}

const typename Mapping<2, 2>::MappingData&
Quad4Mapping::get(const libMesh::Elem* elem)
{
    TBOX_ASSERT(elem->type() == std::get<0>(this->d_quad_key));
    std::copy(d_quad_weights.begin(), d_quad_weights.end(), this->d_JxW.begin());

    // calculate constants in Jacobians here
    const libMesh::Point p0 = elem->point(0);
    const libMesh::Point p1 = elem->point(1);
    const libMesh::Point p2 = elem->point(2);
    const libMesh::Point p3 = elem->point(3);

    const double a_1 = 0.25 * (-p0(0) + p1(0) + p2(0) - p3(0));
    const double b_1 = 0.25 * (-p0(0) - p1(0) + p2(0) + p3(0));
    const double c_1 = 0.25 * (p0(0) - p1(0) + p2(0) - p3(0));
    const double a_2 = 0.25 * (-p0(1) + p1(1) + p2(1) - p3(1));
    const double b_2 = 0.25 * (-p0(1) - p1(1) + p2(1) + p3(1));
    const double c_2 = 0.25 * (p0(1) - p1(1) + p2(1) - p3(1));

    for (unsigned int i = 0; i < this->d_JxW.size(); i++)
    {
        // calculate Jacobians here
        const double x = d_quad_points[i](0);
        const double y = d_quad_points[i](1);

        Eigen::Matrix<double, 2, 2>& contravariant = d_contravariants[i];
        contravariant(0, 0) = a_1 + c_1 * y;
        contravariant(0, 1) = b_1 + c_1 * x;
        contravariant(1, 0) = a_2 + c_2 * y;
        contravariant(1, 1) = b_2 + c_2 * x;

        const double J = contravariant.determinant();

        TBOX_ASSERT(J > 0.0);
        this->d_JxW[i] *= J;
    }

    return this->d_values;
}

Quad9Mapping::Quad9Mapping(const Quad9Mapping::key_type quad_key) : Mapping<2, 2>(quad_key)
{
    // This code utilizes an implementation detail of
    // QBase::tensor_product_quad where the x coordinate increases fastest to
    // reconstruct the 1D quadrature rule
    d_n_oned_q_points = static_cast<std::size_t>(std::round(std::sqrt(d_quad_points.size())));
    TBOX_ASSERT(d_n_oned_q_points * d_n_oned_q_points == d_quad_points.size());
    std::vector<libMesh::Point> oned_points(d_n_oned_q_points);
    for (unsigned int q = 0; q < d_n_oned_q_points; ++q)
    {
        oned_points[q] = d_quad_points[q](0);
    }

    // verify that we really do have a tensor product rule
    unsigned int q = 0;
    for (unsigned int j = 0; j < d_n_oned_q_points; ++j)
    {
        for (unsigned int i = 0; i < d_n_oned_q_points; ++i)
        {
            TBOX_ASSERT(d_quad_points[q] == libMesh::Point(oned_points[i](0), oned_points[j](0)));
            ++q;
        }
    }

    d_phi.resize(3, d_n_oned_q_points);
    d_dphi.resize(3, d_n_oned_q_points);
    for (unsigned int i = 0; i < 3u; ++i)
    {
        for (unsigned int q = 0; q < oned_points.size(); ++q)
        {
            // This class orders the vertices in a different way to make
            // writing tensor products easier: we do a left-to-right ordering
            // 0 - 1 - 2 instead of 0 - 2 - 1.
            constexpr int ibamr_to_libmesh_ordering[3] = { 0, 2, 1 };
            using FE = libMesh::FE<1, libMesh::LAGRANGE>;
            d_phi(i, q) = FE::shape(libMesh::EDGE3, libMesh::SECOND, ibamr_to_libmesh_ordering[i], oned_points[q]);
            d_dphi(i, q) =
                FE::shape_deriv(libMesh::EDGE3, libMesh::SECOND, ibamr_to_libmesh_ordering[i], 0, oned_points[q]);
        }
    }
}

const typename Mapping<2, 2>::MappingData&
Quad9Mapping::get(const libMesh::Elem* elem)
{
    TBOX_ASSERT(elem->type() == std::get<0>(this->d_quad_key));
    std::copy(d_quad_weights.begin(), d_quad_weights.end(), this->d_JxW.begin());

    constexpr std::size_t n_oned_shape_functions = 3;

    // We index points in the following way:
    //
    // i = 2 +--+--+
    //       |     |
    // i = 1 +  +  +
    //       |     |
    // i = 0 +--+--+
    //      j=0 1  2
    //
    // All 2D arrays created here are in row-major order.

    double xs[n_oned_shape_functions][n_oned_shape_functions];
    double ys[n_oned_shape_functions][n_oned_shape_functions];

    libMesh::Point points[n_oned_shape_functions][n_oned_shape_functions];
    points[0][0] = elem->point(0);
    points[0][1] = elem->point(4);
    points[0][2] = elem->point(1);
    points[1][0] = elem->point(7);
    points[1][1] = elem->point(8);
    points[1][2] = elem->point(5);
    points[2][0] = elem->point(3);
    points[2][1] = elem->point(6);
    points[2][2] = elem->point(2);

    // j is the x index, i is the y index
    for (unsigned int i = 0; i < n_oned_shape_functions; ++i)
    {
        for (unsigned int j = 0; j < n_oned_shape_functions; ++j)
        {
            xs[i][j] = points[i][j](0);
            ys[i][j] = points[i][j](1);
        }
    }

    for (unsigned int q = 0; q < this->d_JxW.size(); q++)
    {
        Eigen::Matrix<double, 2, 2>& contravariant = d_contravariants[q];
        contravariant.setZero();
        // Exploit the fact that Quad9 is a tensor product element by indexing
        // the x component of each tensor product shape function with j and
        // the y component with i
        const unsigned int q_point_x = q % d_n_oned_q_points;
        const unsigned int q_point_y = q / d_n_oned_q_points;
        for (unsigned int i = 0; i < n_oned_shape_functions; ++i)
        {
            for (unsigned int j = 0; j < n_oned_shape_functions; ++j)
            {
                contravariant(0, 0) += xs[i][j] * d_dphi(j, q_point_x) * d_phi(i, q_point_y);
                contravariant(0, 1) += xs[i][j] * d_phi(j, q_point_x) * d_dphi(i, q_point_y);
                contravariant(1, 0) += ys[i][j] * d_dphi(j, q_point_x) * d_phi(i, q_point_y);
                contravariant(1, 1) += ys[i][j] * d_phi(j, q_point_x) * d_dphi(i, q_point_y);
            }
        }

        const double J = contravariant.determinant();
        TBOX_ASSERT(J > 0.0);
        this->d_JxW[q] *= J;
    }

    return this->d_values;
}

const typename Mapping<3, 3>::MappingData&
Tet4Mapping::get(const libMesh::Elem* elem)
{
    TBOX_ASSERT(elem->type() == std::get<0>(this->d_quad_key));
    std::copy(d_quad_weights.begin(), d_quad_weights.end(), this->d_JxW.begin());

    // calculate Jacobians here
    const libMesh::Point p0 = elem->point(0);
    const libMesh::Point p1 = elem->point(1);
    const libMesh::Point p2 = elem->point(2);
    const libMesh::Point p3 = elem->point(3);

    Eigen::Matrix<double, 3, 3> contravariant;
    contravariant(0, 0) = p1(0) - p0(0);
    contravariant(0, 1) = p2(0) - p0(0);
    contravariant(0, 2) = p3(0) - p0(0);
    contravariant(1, 0) = p1(1) - p0(1);
    contravariant(1, 1) = p2(1) - p0(1);
    contravariant(1, 2) = p3(1) - p0(1);
    contravariant(2, 0) = p1(2) - p0(2);
    contravariant(2, 1) = p2(2) - p0(2);
    contravariant(2, 2) = p3(2) - p0(2);
    std::fill(d_contravariants.begin(), d_contravariants.end(), contravariant);

    const double J = contravariant.determinant();

    TBOX_ASSERT(J > 0.0);
    for (double& jxw : this->d_JxW) jxw *= J;

    return this->d_values;
}

template class Mapping<1, 1>;
template class Mapping<1, 2>;
template class Mapping<1, 3>;
template class Mapping<2, 2>;
template class Mapping<2, 3>;
template class Mapping<3, 3>;

template class LagrangeMapping<1, 1>;
template class LagrangeMapping<1, 2>;
template class LagrangeMapping<1, 3>;
template class LagrangeMapping<2, 2>;
template class LagrangeMapping<2, 3>;
template class LagrangeMapping<3, 3>;
} // namespace IBTK
