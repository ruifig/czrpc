rem B2 documentation at http://www.boost.org/build/doc/html/bbv2/overview/invocation.html
git clone -b "boost-1.61.0" https://github.com/boostorg/boost.git boost
cd boost
git checkout boost-1.61.0
git submodule update --init --recursive
call bootstrap
.\b2 address-model=64 -j 30