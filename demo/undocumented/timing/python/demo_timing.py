# Copyright (C) 2015 Jan Blechta
#
# This file is part of DOLFIN.
#
# DOLFIN is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# DOLFIN is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.

from __future__ import print_function
from dolfin import *

# Let's solve some variational problem to get non-trivial timings
mesh = UnitSquareMesh(32, 32)
V = FunctionSpace(mesh, "Lagrange", 1)
bc = DirichletBC(V, 0.0, lambda x: near(x[0], 0.0) or near(x[0], 1.0))
u, v = TrialFunction(V), TestFunction(V)
f = Expression("10*exp(-(pow(x[0] - 0.5, 2) + pow(x[1] - 0.5, 2)) / 0.02)")
g = Expression("sin(5*x[0])")
a = inner(grad(u), grad(v))*dx
L = f*v*dx + g*v*ds
u = Function(V)
solve(a == L, u, bc)

# List timings; MPI_MAX taken in parallel
list_timings()

# Get Table object with timings
t = timings()

# Use different MPI reductions
t_sum = MPI.sum(mpi_comm_world(), t)
t_min = MPI.min(mpi_comm_world(), t)
t_max = MPI.max(mpi_comm_world(), t)

# Print aggregate timings to screen
print(t_sum.str(True))
print(t_min.str(True))
print(t_max.str(True))

# Store to XML file on rank 0
if MPI.rank(mpi_comm_world()) == 0:
    f_sum = File(mpi_comm_self(), "timings_mpi_sum.xml")
    f_min = File(mpi_comm_self(), "timings_mpi_min.xml")
    f_max = File(mpi_comm_self(), "timings_mpi_max.xml")
    f_sum << t_sum
    f_min << t_min
    f_max << t_max

# Store separate timings on each rank
f = File(mpi_comm_self(), "timings_rank_%d.xml"
         % MPI.rank(mpi_comm_world()))
f << t
