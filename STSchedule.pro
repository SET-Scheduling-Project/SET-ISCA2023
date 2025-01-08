TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt


QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -O3 -Wall

QMAKE_LFLAGS += -Wl,--stack,64000000 -Wall

HEADERS += \
    include/bitset.h \
    include/bufferusage.h \
    include/cluster.h \
    include/core.h \
    include/coremapping.h \
    include/datalayout.h \
    include/json/json.h \
    include/json/json_autolink.h \
    include/json/json_batchallocator.h \
    include/json/json_config.h \
    include/json/json_features.h \
    include/json/json_forwards.h \
    include/json/json_internalarray.inl \
    include/json/json_internalmap.inl \
    include/json/json_reader.h \
    include/json/json_value.h \
    include/json/json_valueiterator.inl \
    include/json/json_writer.h \
    include/layer.h \
    include/layerengine.h \
    include/ltreenode.h \
    include/network.h \
    include/nns/nns.h \
    include/noc.h \
    include/partition.h \
    include/placement.h \
    include/sa.h \
    include/schnode.h \
    include/util.h

SOURCES += \
    src/bitset.cpp \
    src/bufferusage.cpp \
    src/cluster.cpp \
    src/core.cpp \
    src/coremapping.cpp \
    src/datalayout.cpp \
    src/json/json_reader.cpp \
    src/json/json_value.cpp \
    src/json/json_writer.cpp \
    src/layer.cpp \
    src/layerengine.cpp \
    src/ltreenode.cpp \
    src/main.cpp \
    src/network.cpp \
    src/nns/alexnet.cpp \
    src/nns/darknet19.cpp \
    src/nns/densenet.cpp \
    src/nns/gnmt.cpp \
    src/nns/googlenet.cpp \
    src/nns/incep_resnet.cpp \
    src/nns/llm.cpp \
    src/nns/pnasnet.cpp \
    src/nns/resnet.cpp \
    src/nns/transformer.cpp \
    src/nns/vgg.cpp \
    src/nns/zfnet.cpp \
    src/noc.cpp \
    src/partition.cpp \
    src/placement.cpp \
    src/sa.cpp \
    src/schnode.cpp \
    src/util.cpp

INCLUDEPATH += include/
