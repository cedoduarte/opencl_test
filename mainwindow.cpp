#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <fstream>
#include <sstream>
#include <iostream>
#include <CL/cl.hpp>

using namespace std;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    runOpenCL();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionClose_triggered()
{
    close();
}

void MainWindow::runOpenCL()
{
    cl_int errNum;
    cl_platform_id firstPlatformId;
    cl_context context = nullptr;
    cl_uint numPlatforms;

    // seleccionamos la primera plataforma disponible
    errNum = clGetPlatformIDs(1, &firstPlatformId, &numPlatforms);

    // definimos las propiedades del contexto
    cl_context_properties contextProperties[] = { CL_CONTEXT_PLATFORM, cl_context_properties(firstPlatformId), 0 };

    // creamos un contexto para el primer dispositivo GPU disponible
    context = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &errNum);

    cl_device_id *devices;
    cl_command_queue commandQueue = nullptr;
    size_t deviceBufferSize;

    // obtenemos el tamaño del buffer del dispositivo
    errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES, 0, nullptr, &deviceBufferSize);

    // reservamos memoria para el buffer de dispositivos
    devices = new cl_device_id[deviceBufferSize / sizeof(cl_device_id)];

    // obtenemos el array de dispositivos disponibles
    errNum = clGetContextInfo(context, CL_CONTEXT_DEVICES, deviceBufferSize, devices, nullptr);

    // creamos una cola de comandos para el primer dispositivo disponible
    commandQueue = clCreateCommandQueueWithProperties(context, devices[0], nullptr, nullptr);

    cl_program program;
    string kernelFilename = "kernel_opencl.cl";

    // leemos el archivo kernel_opencl.cl y almacenamos su contenido en un buffer
    ifstream kernelFile(kernelFilename);
    ostringstream oss;
    oss << kernelFile.rdbuf();
    string srcStdStr = oss.str();
    const char *srcStr = srcStdStr.c_str();

    // creamos un objeto de programa del kernel
    program = clCreateProgramWithSource(context, 1, reinterpret_cast<const char **>(&srcStr), nullptr, nullptr);

    // compilamos el kernel
    errNum = clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);

    cl_kernel kernel;

    // creamos una instancia del kernel
    kernel = clCreateKernel(program, "mykernel", nullptr);

    // creamos los array de entrada y salida para el kernel en CPU
    const int ARRAY_SIZE = 10;
    float result[ARRAY_SIZE];
    float a[ARRAY_SIZE];
    float b[ARRAY_SIZE];
    for (int ix = 0; ix < ARRAY_SIZE; ++ix)
    {
        a[ix] = float(ix);
        b[ix] = ix * 2.0f;
    }

    // creamos los objetos de memoria correspondientes de GPU
    cl_mem memObjects[3] = { nullptr, nullptr, nullptr };
    memObjects[0] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) * ARRAY_SIZE, a, nullptr);
    memObjects[1] = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(float) * ARRAY_SIZE, b, nullptr);
    memObjects[2] = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * ARRAY_SIZE, nullptr, nullptr);

    // definimos los argumentos del kernel (result, a, b)
    errNum = clSetKernelArg(kernel, 0, sizeof(cl_mem), &memObjects[0]);
    errNum |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &memObjects[1]);
    errNum |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &memObjects[2]);

    size_t globalWorkSize[1] = { ARRAY_SIZE };
    size_t localWorkSize[1] = { 1 };

    // encola la ejecución del kernel
    errNum = clEnqueueNDRangeKernel(commandQueue, kernel, 1, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);

    // copiamos el buffer de salida del dispositivo y lo retornamos a CPU
    errNum = clEnqueueReadBuffer(commandQueue, memObjects[2], CL_TRUE, 0, ARRAY_SIZE * sizeof(float), result, 0, nullptr, nullptr);

    // imprimimos el resultado
    for (int ix = 0; ix < ARRAY_SIZE; ++ix)
    {
        qDebug() << result[ix];
    }
}
