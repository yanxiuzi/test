/*
  compile this file with :
  nvcc test_cuda_managed_memory.cu -o test_cuda_managed_memory
*/

#include <cuda_runtime.h>
#include <unistd.h>
#include <iostream>

#define SIZE 1024

__global__ void add(int n, float *x, float *y)
{
  int index = threadIdx.x;
  int stride = blockDim.x;

  for (int i = index; i < n; i += stride)
    y[i] = x[i] + y[i];
}

#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true)
{
   if (code != cudaSuccess) 
   {
      fprintf(stderr,"GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
      if (abort) exit(code);
   }
}

// CUDA内核函数，将两个数组相加
__global__ void vec_add(int n, float *x, float *y)
{
  int index = threadIdx.x;
  int stride = blockDim.x;

  for (int i = index; i < n; i += stride)
    y[i] = x[i] + y[i];
}

int main(void)
{
  const int N = 1000;
  const int sz = N*sizeof(float);
  float *x, *y;

  // 检查设备是否支持managed memory
  int device;
  cudaDeviceProp props;
  gpuErrchk(cudaGetDevice(&device));
  gpuErrchk(cudaGetDeviceProperties(&props, device));
  std::cout << "Running test on : " << props.name << std::endl;

  // manually set memory type to pinned memory.
  // props.managedMemory = false;

  if (props.managedMemory)
  {
    // 设备支持managed memory
    std::cout << "Device use managed memory: " << device << std::endl;
    gpuErrchk(cudaMallocManaged(&x, sz));
    gpuErrchk(cudaMallocManaged(&y, sz));
  }
  else
  {
    // 设备不支持managed memory，使用pinned memory
    std::cout << "Device use pinned memory: " << device << std::endl;
    gpuErrchk(cudaMallocHost((void**)&x, sz));
    gpuErrchk(cudaMallocHost((void**)&y, sz));
  }

  // 初始化x和y数组
  for (int i = 0; i < N; i++) {
    x[i] = 1.0f;
    y[i] = 2.0f;
  }

  if (props.managedMemory)
  {
    /// NOTE: jetson不支持 cudaMemPrefetchAsync预取内存, 所以使用cudaStreamAttachMemAsync
    // Prefetch x,y to GPU as they are needed in computation
    gpuErrchk(cudaStreamAttachMemAsync(NULL, x, sz, cudaMemAttachGlobal));
    gpuErrchk(cudaStreamAttachMemAsync(NULL, y, sz, cudaMemAttachGlobal));
  }

  // 执行内核
  int blockSize = 256;
  int numBlocks = (N + blockSize - 1) / blockSize;
  vec_add<<<numBlocks, blockSize>>>(N, x, y);
  // 检查内核启动是否有错误
  gpuErrchk( cudaPeekAtLastError() );

  if (props.managedMemory)
  {
    // Prefetch 'y' to CPU as only 'y' is needed
    gpuErrchk(cudaStreamAttachMemAsync(NULL, y, sz, cudaMemAttachHost));
  }

  // 等待GPU完成工作
  gpuErrchk(cudaDeviceSynchronize());

  // 检查结果
  float maxError = 0.0f;
  for (int i = 0; i < N; i++)
    maxError = fmax(maxError, fabs(y[i]-3.0f));
  std::cout << "Max error: " << maxError << std::endl;

  // 释放内存
  if (props.managedMemory)
  {
    cudaFree(x);
    cudaFree(y);
  }
  else
  {
    cudaFreeHost(x);
    cudaFreeHost(y);
  }

  return 0;
}
