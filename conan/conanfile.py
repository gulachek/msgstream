from conan import ConanFile
from conan.tools.files import chdir, copy, mkdir
from conan.tools.scm import Git
from os.path import join
from conan.tools.gnu import PkgConfigDeps

class BasicConanfile(ConanFile):
    name = "msgstream"
    version = "0.3.1"
    description = "Write messages with boundaries to streams"
    license = "MIT"
    homepage = "https://github.com/gulachek/msgstream"

    def source(self):
        # TODO don't need to copy entire version control system
        git = Git(self)
        git.clone(url="https://github.com/gulachek/msgstream.git", target=".")

    def generate(self):
        pc = PkgConfigDeps(self)
        pc.generate()

    def build(self):
        self.run("npm install")
        self.run("node make.js msgstream")

    def package(self):
        d = self.source_folder
        build = join(d, "build")
        include = join(d, "include")
        buildInclude = join(build, "include")
        copy(self, "*.h", include, join(self.package_folder, "include"))
        copy(self, "msgstream/*.h", buildInclude, join(self.package_folder, "include"))
        copy(self, "libmsgstream.dylib", build, join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.libs = ["msgstream"]
