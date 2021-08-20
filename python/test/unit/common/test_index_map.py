# Copyright (C) 2021 Jørgen S. Dokken
#
# This file is part of DOLFINx (https://www.fenicsproject.org)
#
# SPDX-License-Identifier:    LGPL-3.0-or-later

import dolfinx
from mpi4py import MPI
import numpy as np


def test_index_map_compression():

    mesh = dolfinx.UnitSquareMesh(MPI.COMM_WORLD, 8, 8)
    vertex_map = mesh.topology.index_map(0)
    sl = vertex_map.size_local
    org_ghosts = vertex_map.ghosts
    org_ghost_owners = vertex_map.ghost_owner_rank()

    # Create an index map where only every third local index is saved
    num_owned = 0
    entities = []
    for i in range(sl):
        if i % 4 == 0:
            entities.append(i)
            num_owned += 1
    # Add every fourth ghost
    sub_ghosts = []
    for i in range(len(org_ghosts)):
        if i % 4 == 0:
            entities.append(sl + i)
            sub_ghosts.append(org_ghosts[i])
    sub_ghosts = np.array(sub_ghosts, dtype=np.int64)

    # Create compressed index map
    entities = np.array(entities, dtype=np.int32)
    new_map, org_glob = dolfinx.cpp.common.compress_index_map(vertex_map, entities)

    # Check that the new map has at least as many indices as the input
    # Might have more due to owned indices on other processes
    assert(num_owned <= new_map.size_local)

    # Check that output of compression is sensible
    assert(len(org_glob) == new_map.size_local + new_map.num_ghosts)
    new_sl = new_map.size_local

    # Check that all original ghosts are in the new index map
    # Not necessarily in the same order, as the initial index map does not
    # sort ghosts per process
    new_ghosts = org_glob[new_sl:]
    assert np.isin(new_ghosts, sub_ghosts).all()

    # Check that ghost owner is the same for matching global index
    new_ghost_owners = new_map.ghost_owner_rank()
    for (i, ghost) in enumerate(new_ghosts):
        index = np.flatnonzero(org_ghosts == ghost)[0]
        assert(org_ghost_owners[index] == new_ghost_owners[i])
