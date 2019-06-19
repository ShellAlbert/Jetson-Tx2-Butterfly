#-------------------------------------------------
#
# Project created by QtCreator 2019-06-18T16:05:51
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = butterfly
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11

SOURCES += \
        main.cpp \
        video/zvideocapthread.cpp \
        zgblpara.cpp \
        zmainui.cpp

HEADERS += \
        video/zvideocapthread.h \
        zgblpara.h \
        zmainui.h

INCLUDEPATH += /usr/aarch64-linux-gnu/include

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#for cuda part.
CUDA_SOURCES += $$PWD/cuda/yuv2rgb.cu
CUDA_SDK = /usr/local/cuda
CUDA_DIR = /usr/local/cuda

INCLUDEPATH += $$CUDA_DIR/include
QMAKE_LIBDIR +=$$CUDA_DIR/lib64
CUDA_OBJECTS_DIR = $$PWD/cuda
CUDA_LIBS =   -lpthread -lv4l2 -lEGL -lGLESv2 -lX11 -lnvbuf_utils -lnvjpeg -lnvosd -ldrm -lcuda -lcudart -lnvinfer -lnvparsers \
              -L"/home/zhangshaoyan/JetsonTx2/RootfsOnTegra//usr/local/cuda/lib64" \
              -L"/home/zhangshaoyan/JetsonTx2/RootfsOnTegra/lib/aarch64-linux-gnu" \
              -L"/home/zhangshaoyan/JetsonTx2/RootfsOnTegra/usr/lib/aarch64-linux-gnu" \
              -L"/home/zhangshaoyan/JetsonTx2/RootfsOnTegra/usr/lib/aarch64-linux-gnu/tegra" \
              --sysroot=/home/zhangshaoyan/JetsonTx2/RootfsOnTegra
CUDA_HEADERS =-Xcompiler -I"/home/zhangshaoyan/JetsonTx2/tegra_multimedia_api/include" \
              -Xcompiler -I"/home/zhangshaoyan/JetsonTx2/tegra_multimedia_api/include/libjpeg-8b" \
              -Xcompiler -I"/home/zhangshaoyan/JetsonTx2/tegra_multimedia_api/samples/common/algorithm/cuda" \
              -Xcompiler -I"/home/zhangshaoyan/JetsonTx2/tegra_multimedia_api/samples/common/algorithm/trt" \
              -Xcompiler -I"/home/zhangshaoyan/JetsonTx2/RootfsOnTegra//usr/local/cuda/include" \
              -Xcompiler -I"/home/zhangshaoyan/JetsonTx2/RootfsOnTegra/usr/include/aarch64-linux-gnu" \
              -Xcompiler -I"/home/zhangshaoyan/JetsonTx2/RootfsOnTegra/usr/include/libdrm"  \

LIBS += $$CUDA_LIBS


cuda.input = CUDA_SOURCES
cuda.output = $$CUDA_OBJECTS_DIR/${QMAKE_FILE_BASE}.o
cuda.commands = $$CUDA_DIR/bin/nvcc -ccbin aarch64-linux-gnu-g++ \
                        -Xcompiler --sysroot=/home/zhangshaoyan/JetsonTx2/RootfsOnTegra  \
                        $$CUDA_HEADERS \
                        -gencode arch=compute_53,code=sm_53 \
                        -gencode arch=compute_62,code=sm_62 \
                        -gencode arch=compute_72,code=sm_72 \
                        -gencode arch=compute_72,code=compute_72 \
                        -c -o ${QMAKE_FILE_OUT} ${QMAKE_FILE_NAME}
cuda.dependency_type = TYPE_C
QMAKE_EXTRA_COMPILERS += cuda

