// Copyright (C) 2007-2014 Anders Logg
//
// This file is part of DOLFIN (https://www.fenicsproject.org)
//
// SPDX-License-Identifier:    LGPL-3.0-or-later

#pragma once

#include "FormCoefficients.h"
#include "FormIntegrals.h"
#include <dolfin/common/types.h>
#include <map>
#include <memory>
#include <vector>

// Forward declaration
namespace ufc
{
class form;
}

namespace dolfin
{

class FunctionSpace;
class GenericFunction;
class Mesh;
template <typename T>
class MeshFunction;

namespace fem
{

/// Base class for UFC code generated by FFC for DOLFIN with option -l.

/// @verbatim embed:rst:leading-slashes
/// A note on the order of trial and test spaces: FEniCS numbers
/// argument spaces starting with the leading dimension of the
/// corresponding tensor (matrix). In other words, the test space is
/// numbered 0 and the trial space is numbered 1. However, in order
/// to have a notation that agrees with most existing finite element
/// literature, in particular
///
///     a = a(u, v)
///
/// the spaces are numbered from right to
///
///     a: V_1 x V_0 -> R
///
/// .. note::
///
///     Figure out how to write this in math mode without it getting
///     messed up in the Python version.
///
/// This is reflected in the ordering of the spaces that should be
/// supplied to generated subclasses. In particular, when a bilinear
/// form is initialized, it should be initialized as
///
/// .. code-block:: c++
///
///     a(V_1, V_0) = ...
///
/// where ``V_1`` is the trial space and ``V_0`` is the test space.
/// However, when a form is initialized by a list of argument spaces
/// (the variable ``function_spaces`` in the constructors below, the
/// list of spaces should start with space number 0 (the test space)
/// and then space number 1 (the trial space).
/// @endverbatim

class Form
{
public:
  /// Create form (shared data)
  ///
  /// @param[in] ufc_form (ufc::form)
  ///         The UFC form.
  /// @param[in] function_spaces (std::vector<_FunctionSpace_>)
  ///         Vector of function spaces.
  Form(std::shared_ptr<const ufc::form> ufc_form,
       std::vector<std::shared_ptr<const FunctionSpace>> function_spaces);

  /// Destructor
  virtual ~Form();

  /// Return rank of form (bilinear form = 2, linear form = 1,
  /// functional = 0, etc)
  ///
  /// @return std::size_t
  ///         The rank of the form.
  std::size_t rank() const;

  /// Return original coefficient position for each coefficient (0
  /// <= i < n)
  ///
  /// @return std::size_t
  ///         The position of coefficient i in original ufl form coefficients.
  std::size_t original_coefficient_position(std::size_t i) const;

  /// Return the size of the element tensor, needed to create temporary space
  /// for assemblers. If the largest number of per-element dofs in FunctionSpace
  /// i is N_i, then for a linear form this is N_0, and for a bilinear form,
  /// N_0*N_1.
  ///
  /// @return std::size_t
  ///         The maximum number of values in a local element tensor
  ///
  /// FIXME: remove this, Assembler should calculate or put in utils
  std::size_t max_element_tensor_size() const;

  /// Set mesh, necessary for functionals when there are no function
  /// spaces
  ///
  /// @param[in] mesh (_Mesh_)
  ///         The mesh.
  void set_mesh(std::shared_ptr<const Mesh> mesh);

  /// Extract common mesh from form
  ///
  /// @return Mesh
  ///         Shared pointer to the mesh.
  std::shared_ptr<const Mesh> mesh() const;

  /// Return function space for given argument
  ///
  /// @param  i (std::size_t)
  ///         Index
  ///
  /// @return FunctionSpace
  ///         Function space shared pointer.
  std::shared_ptr<const FunctionSpace> function_space(std::size_t i) const;

  /// Return function spaces for arguments
  ///
  /// @return    std::vector<_FunctionSpace_>
  ///         Vector of function space shared pointers.
  std::vector<std::shared_ptr<const FunctionSpace>> function_spaces() const;

  /// Return cell domains (zero pointer if no domains have been
  /// specified)
  ///
  /// @return     _MeshFunction_ <std::size_t>
  ///         The cell domains.
  std::shared_ptr<const MeshFunction<std::size_t>> cell_domains() const;

  /// Return exterior facet domains (zero pointer if no domains have
  /// been specified)
  ///
  /// @return     std::shared_ptr<_MeshFunction_ <std::size_t>>
  ///         The exterior facet domains.
  std::shared_ptr<const MeshFunction<std::size_t>>
  exterior_facet_domains() const;

  /// Return interior facet domains (zero pointer if no domains have
  /// been specified)
  ///
  /// @return     _MeshFunction_ <std::size_t>
  ///         The interior facet domains.
  std::shared_ptr<const MeshFunction<std::size_t>>
  interior_facet_domains() const;

  /// Return vertex domains (zero pointer if no domains have been
  /// specified)
  ///
  /// @return     _MeshFunction_ <std::size_t>
  ///         The vertex domains.
  std::shared_ptr<const MeshFunction<std::size_t>> vertex_domains() const;

  /// Set cell domains
  ///
  /// @param[in]    cell_domains (_MeshFunction_ <std::size_t>)
  ///         The cell domains.
  void set_cell_domains(
      std::shared_ptr<const MeshFunction<std::size_t>> cell_domains);

  /// Set exterior facet domains
  ///
  ///  @param[in]   exterior_facet_domains (_MeshFunction_ <std::size_t>)
  ///         The exterior facet domains.
  void set_exterior_facet_domains(
      std::shared_ptr<const MeshFunction<std::size_t>> exterior_facet_domains);

  /// Set interior facet domains
  ///
  ///  @param[in]   interior_facet_domains (_MeshFunction_ <std::size_t>)
  ///         The interior facet domains.
  void set_interior_facet_domains(
      std::shared_ptr<const MeshFunction<std::size_t>> interior_facet_domains);

  /// Set vertex domains
  ///
  ///  @param[in]   vertex_domains (_MeshFunction_ <std::size_t>)
  ///         The vertex domains.
  void set_vertex_domains(
      std::shared_ptr<const MeshFunction<std::size_t>> vertex_domains);

  /// Access coefficients (non-const)
  FormCoefficients& coeffs() { return _coeffs; }

  /// Access coefficients (const)
  const FormCoefficients& coeffs() const { return _coeffs; }

  /// Access form integrals (const)
  const FormIntegrals& integrals() const { return _integrals; }

private:
  // Integrals associated with the Form
  FormIntegrals _integrals;

  // Coefficients associated with the Form
  FormCoefficients _coeffs;

  // Function spaces (one for each argument)
  std::vector<std::shared_ptr<const FunctionSpace>> _function_spaces;

  // The mesh (needed for functionals when we don't have any spaces)
  std::shared_ptr<const Mesh> _mesh;

  // Domain markers for cells
  std::shared_ptr<const MeshFunction<std::size_t>> dx;

  // Domain markers for exterior facets
  std::shared_ptr<const MeshFunction<std::size_t>> ds;

  // Domain markers for interior facets
  std::shared_ptr<const MeshFunction<std::size_t>> dS;

  // Domain markers for vertices
  std::shared_ptr<const MeshFunction<std::size_t>> dP;
};
}
}