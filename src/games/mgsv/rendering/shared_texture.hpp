#ifndef RENODX_GAMES_MGSV_RENDERING_SHARED_TEXTURE_HPP_
#define RENODX_GAMES_MGSV_RENDERING_SHARED_TEXTURE_HPP_

#include <memory>
#include <shared_mutex>
#include <utility>

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace mgsv::rendering {

using SharedHandle = std::unique_ptr<void, decltype([](void* handle) { CloseHandle(handle); })>;

// One shareable D3D11 texture's exported NT handle and its cached D3D12 import.
// Bridge command slots can rotate without opening the same image again. Each
// slot keeps its own COM references, so replacing this cache cannot release an
// image the GPU is still using.
struct SharedTexture {
  std::shared_mutex mutex;
  SharedHandle handle;
  Microsoft::WRL::ComPtr<ID3D12Device> device;
  Microsoft::WRL::ComPtr<ID3D12Resource> resource;

  explicit SharedTexture(SharedHandle handle) : handle(std::move(handle)) {}
};

// DXGI permits one exported NT handle per allocation. The producer owns that
// handle and passes it with the texture so every consumer can reuse it.
inline std::shared_ptr<SharedTexture> OwnSharedHandle(HANDLE handle) {
  if (handle == nullptr) return {};
  return std::make_shared<SharedTexture>(SharedHandle(handle));
}

inline std::shared_ptr<SharedTexture> ExportTexture(ID3D11Texture2D* texture) {
  Microsoft::WRL::ComPtr<IDXGIResource1> resource;
  HANDLE handle = nullptr;
  if (texture == nullptr || FAILED(texture->QueryInterface(IID_PPV_ARGS(&resource)))
      || FAILED(resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &handle))) return {};
  return OwnSharedHandle(handle);
}

}  // namespace mgsv::rendering

#endif  // RENODX_GAMES_MGSV_RENDERING_SHARED_TEXTURE_HPP_
