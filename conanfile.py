from conans import python_requires
import os

pmi_helper = python_requires('pmi_common_recipe/1.0.7@pmi/develop')


class QtCommonConan(pmi_helper.CommonRecipeConan):
    scm = {
        'type': 'git',
        'url': 'https://bitbucket.org/ykil/qt_common.git',
        'revision': 'auto'
    }
    name = 'qt_common'
    version = pmi_helper.pmi_version_from_cmake()
    description = 'qt_common'
    requires = (
        'qt/5.9.2@pmi/develop',
        'boost/1.66.0@pmi/develop',
        'pmi_common_stable/2019.5.14@pmi/develop',
        'qwt/6.1.3@pel/develop',
        'eigen/3.3.5@pel/develop',
        'sqlite3/3.20.0@pel/develop',
        'vendor_api_qtc/0.0.1@pel/develop',
        'pwiz_pmi/1.0.0@pel/develop'
    )
    options = {
        'shared': [True, False],
        'autotest': [True, False],
    }
    default_options= '''
        shared=True
        autotest=True
        boost:header_only=True
    '''


    def build_requirements(self):
        self.build_requires('pel_scripts/2.0.5@pmi/develop')


    def requirements(self):
        # we only have 32 bit version of qhttpengine
        if self.settings.arch == 'x86':
            self.requires('qhttpengine/0.1.0@pel/develop')


    def imports(self):
        self.pmi_import_vendor_api()
        self.pmi_import_qt_plugins(['sqldrivers', 'platforms'])
        

    def deploy(self):
        #deploy current package
        for p in ['PMi-*.exe', '*.dll']:
            self.copy(p, dst='', src='bin')
        
        #deploy dependencies
        self.pmi_deploy_vendor_api()
        self.pmi_deploy_qt_dlls(['Core', 'Gui', 'Widgets', 'OpenGL', 'Network', 
            'XmlPatterns', 'Sql', 'Xml', 'Svg', 'PrintSupport', 'Qml', 'Concurrent'])
        
        for p in ['pmi_common_stable', 'qwt', 'sqlite3', 'openssl']:
            self.copy_deps('*.dll', root_package=p, src='bin')
        self.copy_deps('*', root_package='pwiz_pmi', dst='pwiz-pmi', src='bin')
       
               
    def package_info(self):
        self.user_info.ROOT_PATH = self.package_folder

        lib_postfix = self.pmi_lib_postfix()
        for p in ['pmi_common_core_mini', 'pmi_common_ms', 'pmi_common_tiles', 'pmi_common_widgets']:
            self.cpp_info.includedirs.append('include/{}'.format(p))
        
        for lib in ['pmi_common_core_mini', 'pmi_common_ms', 'pmi_common_tiles', 'pmi_common_widgets']:
            self.cpp_info.libs.append('{}{}'.format(lib, lib_postfix))

        if self.settings.os == 'Windows':
            self.cpp_info.libs.append('psapi')

