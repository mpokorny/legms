# Copyright 2013-2020 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

# ----------------------------------------------------------------------------
# If you submit this package back to Spack as a pull request,
# please first remove this boilerplate and all FIXME comments.
#
# This is a template package file for Spack.  We've put "FIXME"
# next to all the things you'll want to change. Once you've handled
# them, you can save this file and test your package like this:
#
#     spack install hyperion
#
# You can edit this file again by typing:
#
#     spack edit hyperion
#
# See the Spack documentation for more information on packaging.
# ----------------------------------------------------------------------------

from spack import *


class Hyperion(CMakePackage):
    """High performance radio astronomy data processing prototyping"""

    # Add a proper url for your package's homepage here.
    homepage = "https://www.nrao.edu"
    git      = "https://github.com/mpokorny/hyperion.git"

    maintainers = ['mpokorny']

    version('master', branch='master', submodules=True)

    # CMake build dependency
    depends_on('cmake@3.13:', type='build')

    # Top-level hyperion variants
    variant('max_dim', default='7', description='Maximum index space rank',
            values=('4','5','6','7','8','9'), multi=False)
    variant('casacore', default=True, description='Enable casacore support')
    variant('hdf5', default=True, description='Enable HDF5 support')
    variant('yaml', default=True, description='Enable YAML support')
    variant('llvm', default=False, description='Enable LLVM support')
    variant('cuda', default=False, description='Enable CUDA support')
    variant('cuda_arch', default=(), description='Target CUDA compute capabilities',
            values=('30','32','35','50','52','53','60','61','62','70','72','75'), multi=True)
    variant('kokkos', default=False, description='Enable Kokkos support')
    variant('openmp', default=False, description='Enable OpenMP support (via Kokkos only)')
    variant('debug', default=False, description='Enable debug flags')

    # Variants for Legion sub-project
    variant('lg_debug', default=False, description='Enable Legion debug flags')
    variant('lg_output_level', default='debug', description='Legion compile time logging level',
            values=('spew', 'debug', 'info', 'print', 'warning', 'error', 'fatal', 'none'), multi=False)
    variant('lg_bounds_checks', default=False, description='Enable Legion bounds checks')
    variant('lg_privilege_checks', default=False, description='Enable Legion privilege checks')
    variant('lg_mpi', default=False, description='Legion MPI backend')
    variant('lg_gasnet', default=False, description='Legion GASNet backend')
    variant('lg_hijack_cudart', default=True, description='Enable Legion CUDA runtime hijack')
    variant('lg_build', default=(), description='Extra Legion build targets',
            values=('all', 'apps', 'bindings', 'examples', 'tutorial', 'tests'), multi=True)

    # Remaining dependencies
    depends_on('zlib')
    depends_on('casacore', when='+casacore')
    depends_on('hdf5+threadsafe+szip~mpi', when='+hdf5')
    depends_on('yaml-cpp', when='+yaml')
    depends_on('llvm@6.0.1', when='+llvm')
    depends_on('cuda', when='+cuda', type=('build', 'link', 'run'))
    depends_on('mpi', when='+lg_mpi')
    depends_on('gasnet+par~aligned-segments segment=fast', when='+lg_gasnet')
    depends_on('kokkos+openmp std=17', when='+kokkos+openmp~cuda')
    depends_on('kokkos std=17', when='+kokkos~cuda~openmp')
    # FIXME: don't require nvcc_wrapper when compiling with Clang
    for arch in ('30','32','35','50','52','53','60','61','62','70','72','75'):
        depends_on(f'kokkos+openmp+cuda+cuda_lambda+cuda_relocatable_device_code cuda_arch={arch} +wrapper std=14 ~cuda_host_init_check',
                   when=f'+kokkos+openmp+cuda cuda_arch={arch}')
        depends_on(f'kokkos+cuda+cuda_lambda+cuda_relocatable_device_code cuda_arch={arch} +wrapper std=14 ~cuda_host_init_check',
                   when=f'+kokkos+cuda~openmp cuda_arch={arch}')

    # not sure how to make this a dependency only for running tests
    depends_on('python@3:', type='run')

    def cmake_args(self):
        args = []
        spec = self.spec

        args.append(self.define_from_variant('USE_CASACORE', 'casacore'))
        args.append(self.define_from_variant('USE_HDF5', 'hdf5'))
        args.append(self.define_from_variant('MAX_DIM', 'max_dim'))
        args.append(self.define_from_variant('USE_OPENMP', 'openmp'))
        args.append(self.define_from_variant('USE_CUDA', 'cuda'))
        args.append(self.define_from_variant('USE_KOKKOS', 'kokkos'))
        args.append(self.define_from_variant('gridder_USE_YAML', 'yaml'))
        args.append(self.define_from_variant('Legion_BOUNDS_CHECKS', 'lg_bounds_checks'))
        args.append(self.define_from_variant('Legion_PRIVILEGE_CHECKS', 'lg_privilege_checks'))

        for lv in ('spew', 'debug', 'info', 'print', 'warning', 'error', 'fatal', 'none'):
            if 'lg_output_level=' + lv in spec:
                args.append('-DLegion_OUTPUT_LEVEL=' + lv.upper())

        for bld in ('all', 'apps', 'bindings', 'examples', 'tutorial', 'tests'):
            if 'lg_build=' + bld in spec:
                args.append('-DLegion_BUILD_' + bld.upper() + '=ON')

        backends = ''
        if '+lg_mpi' in spec:
            backends = backends + ',mpi'
        if '+lg_gasnet' in spec:
            backends = backends + ',gasnet1'
        if len(backends) > 0:
            args.append('-DLegion_NETWORKS=' + backends[1:])

        if '+cuda' in spec:
            archs = ''
            for a in ['30','32','35','50','52','53','60','61','62','70','72','75']:
                if 'cuda_arch=' + a in spec:
                    archs = archs + ',' + a
            if len(archs) > 0:
                args.append('-DCUDA_ARCH=' + archs[1:])
            args.append(self.define_from_variant('Legion_HIJACK_CUDART', 'lg_hijack_cudart'))

        if '+debug' in spec:
            args.append('-DCMAKE_BUILD_TYPE=Debug')
        if '+lg_debug' in spec:
            args.append('-DLegion_CMAKE_BUILD_TYPE=Debug')

        args.append('-DBUILD_ARCH:STRING=none')
        args.append("-DCMAKE_CXX_COMPILER=%s" % self.spec["kokkos"].kokkos_cxx)
        return args
