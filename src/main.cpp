#include <CL/cl.h>
#include <libclew/ocl_init.h>
#include <libutils/timer.h>
#include <libutils/fast_random.h>

#include <vector>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <cassert>


template <typename T>
std::string to_string(T value)
{
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

void reportError(cl_int err, const std::string &filename, int line)
{
    if (CL_SUCCESS == err)
        return;

    // Таблица с кодами ошибок:
    // libs/clew/CL/cl.h:103
    // P.S. Быстрый переход к файлу в CLion: Ctrl+Shift+N -> cl.h (или даже с номером строки: cl.h:103) -> Enter
    std::string message = "OpenCL error code " + to_string(err) + " encountered at " + filename + ":" + to_string(line);
    throw std::runtime_error(message);
}

#define OCL_CHECK(err) reportError(err, __FILE__, __LINE__)
#define OCL_SAFE_CALL(expr) reportError(expr, __FILE__, __LINE__)

void get_best_device(cl_platform_id& best_platform, cl_device_id& best_device)
{
    cl_uint platformsCount = 0;
    OCL_SAFE_CALL(clGetPlatformIDs(0, nullptr, &platformsCount));
    std::cout << "Number of OpenCL platforms: " << platformsCount << std::endl;

    std::vector<cl_platform_id> platforms(platformsCount);
    OCL_SAFE_CALL(clGetPlatformIDs(platformsCount, platforms.data(), nullptr));

    for (int platformIndex = 0; platformIndex < platformsCount; ++platformIndex) {
        cl_platform_id platform = platforms[platformIndex];

        cl_uint devicesCount = 0;
        OCL_SAFE_CALL(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 0, nullptr , &devicesCount));

        std::vector<cl_device_id> devices(devicesCount);
        OCL_SAFE_CALL(clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, devicesCount, devices.data(), nullptr));

        for (int deviceIndex = 0; deviceIndex < devicesCount; ++deviceIndex)
        {
            cl_device_id device = devices[deviceIndex];

            cl_device_type deviceType;
            OCL_SAFE_CALL(clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(cl_device_type), &deviceType, nullptr));
            switch (deviceType)
            {
                case CL_DEVICE_TYPE_CPU:
                    std::cout << "CPU is found" << std::endl;
                    best_device = device;
                    best_platform = platform;
                    break; // not return, continue to find GPU

                case CL_DEVICE_TYPE_GPU:
                    std::cout << "GPU is found"  << std::endl;
                    best_device = device;
                    best_platform = platform;
                    return;

                default:
                    std::cout << "            Device type is unknown..."  << std::endl;
                    break;

            }
        }
    }

}

int main()
{    
    // Пытаемся слинковаться с символами OpenCL API в runtime (через библиотеку clew)
    if (!ocl_init())
        throw std::runtime_error("Can't init OpenCL driver!");

    // TODO 1 По аналогии с заданием Example0EnumDevices узнайте какие есть устройства, и выберите из них какое-нибудь
    cl_platform_id platform_id;
    cl_device_id  device_id;
    get_best_device(platform_id, device_id);
    // (если есть хоть одна видеокарта - выберите ее, если нету - выбирайте процессор)

    // TODO 2 Создайте контекст с выбранным устройством
    cl_int errcode = 0;
    cl_context context = clCreateContext(nullptr, 1, &device_id, nullptr, nullptr, &errcode);
    OCL_CHECK(errcode);
    // См. документацию https://www.khronos.org/registry/OpenCL/sdk/1.2/docs/man/xhtml/ -> OpenCL Runtime -> Contexts -> clCreateContext
    // Не забывайте проверять все возвращаемые коды на успешность (обратите внимание что в данном случае метод возвращает
    // код по переданному аргументом errcode_ret указателю)
    // И хорошо бы сразу добавить в конце clReleaseContext (да, не очень RAII, но это лишь пример)

    // TODO 3 Создайте очередь выполняемых команд в рамках выбранного контекста и устройства
    // См. документацию https://www.khronos.org/registry/OpenCL/sdk/1.2/docs/man/xhtml/ -> OpenCL Runtime -> Runtime APIs -> Command Queues -> clCreateCommandQueue
    // Убедитесь что в соответствии с документацией вы создали in-order очередь задач
    // И хорошо бы сразу добавить в конце clReleaseQueue
    cl_command_queue_properties  queue_properties = 0;
    cl_command_queue queue = clCreateCommandQueue(context, device_id, queue_properties, &errcode);
    OCL_CHECK(errcode);


    unsigned int n = 100*1000*1000;
    // Создаем два массива псевдослучайных данных для сложения и массив для будущего хранения результата
    std::vector<float> as(n, 0);
    std::vector<float> bs(n, 0);
    std::vector<float> cs(n, 0);
    FastRandom r(n);
    for (unsigned int i = 0; i < n; ++i) {
        as[i] = r.nextf();
        bs[i] = r.nextf();
    }
    std::cout << "Data generated for n=" << n << "!" << std::endl;

    // TODO 4 Создайте три буфера в памяти устройства (в случае видеокарты - в видеопамяти - VRAM) - для двух суммируемых массивов as и bs (они read-only) и для массива с результатом cs (он write-only)
    // См. Buffer Objects -> clCreateBuffer
    // Размер в байтах соответственно можно вычислить через sizeof(float)=4 и тот факт что чисел в каждом массиве - n штук
    // Данные в as и bs можно прогрузить этим же методом скопировав данные из host_ptr=as.data() (и не забыв про битовый флаг на это указывающий)
    // или же через метод Buffer Objects -> clEnqueueWriteBuffer
    // И хорошо бы сразу добавить в конце clReleaseMemObject (аналогично все дальнейшие ресурсы вроде OpenCL под-программы, кернела и т.п. тоже нужно освобождать)
    cl_mem_flags buffer_flags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR;
    cl_mem as_gpu = clCreateBuffer(context, buffer_flags, sizeof(float) * n, as.data(), &errcode);
    OCL_CHECK(errcode);

    cl_mem bs_gpu = clCreateBuffer(context, buffer_flags, sizeof(float) * n, bs.data(), &errcode);
    OCL_CHECK(errcode);

    cl_mem_flags cs_buffer_flags = CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR;
    cl_mem cs_gpu = clCreateBuffer(context, cs_buffer_flags, sizeof(float) * n, cs.data(), &errcode);
    OCL_CHECK(errcode);

    // TODO 6 Выполните TODO 5 (реализуйте кернел в src/cl/aplusb.cl)
    // затем убедитесь что выходит загрузить его с диска (убедитесь что Working directory выставлена правильно - см. описание задания)
    // напечатав исходники в консоль (if проверяет что удалось считать хоть что-то)
    std::string kernel_sources;
    {
        std::ifstream file("src/cl/aplusb.cl");
        kernel_sources = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        if (kernel_sources.size() == 0) {
            throw std::runtime_error("Empty source file! May be you forgot to configure working directory properly?");
        }
        // std::cout << kernel_sources << std::endl;
    }

    // TODO 7 Создайте OpenCL-подпрограмму с исходниками кернела
    // см. Runtime APIs -> Program Objects -> clCreateProgramWithSource
    // у string есть метод c_str(), но обратите внимание что передать вам нужно указатель на указатель
    const char* str = kernel_sources.c_str();
    size_t length = kernel_sources.length();
    cl_program program = clCreateProgramWithSource(context, 1, &str, &length, &errcode);
    OCL_CHECK(errcode);

    // TODO 8 Теперь скомпилируйте программу и напечатайте в консоль лог компиляции
    // см. clBuildProgram
    cl_int build_errcode = clBuildProgram(program, 1, &device_id,nullptr, nullptr, nullptr); // we aren't checking error code, as at first print build log and then terminate program if error

    // А так же напечатайте лог компиляции (он будет очень полезен, если в кернеле есть синтаксические ошибки - т.е. когда clBuildProgram вернет CL_BUILD_PROGRAM_FAILURE)
    // см. clGetProgramBuildInfo

    size_t log_size = 0;
    OCL_SAFE_CALL(clGetProgramBuildInfo(program, device_id,CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_size));

    std::vector<char> log(log_size, 0);
    OCL_SAFE_CALL(clGetProgramBuildInfo(program, device_id,CL_PROGRAM_BUILD_LOG, log_size, log.data(), nullptr));
    if (log_size > 1)
    {
        std::cout << "Compilation Log:" << std::endl;
        std::cout << log.data() << std::endl;
    }
    OCL_CHECK(build_errcode);

    // TODO 9 Создайте OpenCL-kernel в созданной подпрограмме (в одной подпрограмме может быть несколько кернелов, но в данном случае кернел один)
    // см. подходящую функцию в Runtime APIs -> Kernel Objects
    cl_kernel kernel = clCreateKernel(program, "aplusb", &errcode);
    OCL_CHECK(errcode);

    // TODO 10 Выставите все аргументы в кернеле через clSetKernelArg (as_gpu, bs_gpu, cs_gpu и число значений, убедитесь что тип количества элементов такой же в кернеле)
    {
        unsigned int i = 0;
        OCL_SAFE_CALL(clSetKernelArg(kernel, i++, sizeof(cl_mem), &as_gpu));
        OCL_SAFE_CALL(clSetKernelArg(kernel, i++, sizeof(cl_mem), &bs_gpu));
        OCL_SAFE_CALL(clSetKernelArg(kernel, i++, sizeof(cl_mem), &cs_gpu));
        OCL_SAFE_CALL(clSetKernelArg(kernel, i++, sizeof(unsigned int), &n));
    }

    // TODO 11 Выше увеличьте n с 1000*1000 до 100*1000*1000 (чтобы дальнейшие замеры были ближе к реальности)
    
    // TODO 12 Запустите выполнения кернела:
    // - С одномерной рабочей группой размера 128
    // - В одномерном рабочем пространстве размера roundedUpN, где roundedUpN - наименьшее число кратное 128 и при этом не меньшее n
    // - см. clEnqueueNDRangeKernel
    // - Обратите внимание что чтобы дождаться окончания вычислений (чтобы знать когда можно смотреть результаты в cs_gpu) нужно:
    //   - Сохранить событие "кернел запущен" (см. аргумент "cl_event *event")
    //   - Дождаться завершения полунного события - см. в документации подходящий метод среди Event Objects
    {
        size_t workGroupSize = 128;
        size_t global_work_size = (n + workGroupSize - 1) / workGroupSize * workGroupSize;
        size_t local_work_offset = workGroupSize; //
        timer t; // Это вспомогательный секундомер, он замеряет время своего создания и позволяет усреднять время нескольких замеров
        for (unsigned int i = 0; i < 20; ++i)
        {
            cl_event kernel_run;
            clEnqueueNDRangeKernel(queue,kernel,1, nullptr, &global_work_size, &local_work_offset, 0, nullptr, &kernel_run);
            clWaitForEvents(1, &kernel_run);
            t.nextLap(); // При вызове nextLap секундомер запоминает текущий замер (текущий круг) и начинает замерять время следующего круга
        }
        // Среднее время круга (вычисления кернела) на самом деле считаются не по всем замерам, а лишь с 20%-перцентайля по 80%-перцентайль (как и стандартное отклониение)
        // подробнее об этом - см. timer.lapsFiltered
        // P.S. чтобы в CLion быстро перейти к символу (функции/классу/много чему еще) достаточно нажать Ctrl+Shift+Alt+N -> lapsFiltered -> Enter
        std::cout << "Kernel average time: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        
        // TODO 13 Рассчитайте достигнутые гигафлопcы:
        // - Всего элементов в массивах по n штук
        // - Всего выполняется операций: операция a+b выполняется n раз
        // - Флопс - это число операций с плавающей точкой в секунду
        // - В гигафлопсе 10^9 флопсов
        // - Среднее время выполнения кернела равно t.lapAvg() секунд
        std::cout << "GFlops: " << (n / t.lapAvg()) / 1e9 << std::endl;

        // TODO 14 Рассчитайте используемую пропускную способность обращений к видеопамяти (в гигабайтах в секунду)
        // - Всего элементов в массивах по n штук
        // - Размер каждого элемента sizeof(float)=4 байта
        // - Обращений к видеопамяти т.о. 2*n*sizeof(float) байт на чтение и 1*n*sizeof(float) байт на запись, т.е. итого 3*n*sizeof(float) байт
        // - В гигабайте 1024*1024*1024 байт
        // - Среднее время выполнения кернела равно t.lapAvg() секунд
        std::cout << "VRAM bandwidth: " << (3*n*sizeof(float) / t.lapAvg() )/ (1024*1024*1024) << " GB/s" << std::endl;
    }

    // TODO 15 Скачайте результаты вычислений из видеопамяти (VRAM) в оперативную память (RAM) - из cs_gpu в cs (и рассчитайте скорость трансфера данных в гигабайтах в секунду)
    {
        timer t;
        for (unsigned int i = 0; i < 20; ++i) {
            clEnqueueReadBuffer(queue, cs_gpu,CL_TRUE,0,n*sizeof(float),cs.data(),0, nullptr, nullptr);
            t.nextLap();
        }
        std::cout << "Result data transfer time: " << t.lapAvg() << "+-" << t.lapStd() << " s" << std::endl;
        std::cout << "VRAM -> RAM bandwidth: " << (n * sizeof(float) / t.lapAvg()) / (1024*1024*1024) << " GB/s" << std::endl;
    }

    // TODO 16 Сверьте результаты вычислений со сложением чисел на процессоре (и убедитесь, что если в кернеле сделать намеренную ошибку, то эта проверка поймает ошибку)
    for (unsigned int i = 0; i < n; ++i) {
        if (cs[i] != as[i] + bs[i]) {
            char ar[50];
            sprintf(ar, "%d actual -- %f  expected -- %f ", i, cs[i], as[i]+bs[i]);
            throw std::runtime_error("CPU and GPU results differ! At position " + std::string(ar));
        }
    }

    clReleaseMemObject(as_gpu);
    clReleaseMemObject(bs_gpu);
    clReleaseMemObject(cs_gpu);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(queue);
    clReleaseProgram(program);
    clReleaseContext(context);
    return 0;
}
