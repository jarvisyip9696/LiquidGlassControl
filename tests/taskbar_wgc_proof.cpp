#define _WIN32_WINNT 0x0A00
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#undef GetCurrentTime
#include <d3d11.h>
#include <windows.graphics.capture.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <vector>
#include <cstdio>
extern "C" HRESULT WINAPI CreateDirect3D11DeviceFromDXGIDevice(IDXGIDevice*,IInspectable**);
struct TextureAccess:IUnknown{virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID,void**)=0;};
struct AutoHideProofLease {
 HWND window{};UINT oldState=0,testState=0;bool owned=false;
 void expand(HWND h){window=h;APPBARDATA data{sizeof(data)};data.hWnd=h;oldState=UINT(SHAppBarMessage(ABM_GETSTATE,&data));testState=oldState&~UINT(ABS_AUTOHIDE);
  if(oldState!=testState){data.lParam=testState;SHAppBarMessage(ABM_SETSTATE,&data);owned=true;Sleep(700);}}
 ~AutoHideProofLease(){if(owned){APPBARDATA data{sizeof(data)};data.hWnd=window;if(UINT(SHAppBarMessage(ABM_GETSTATE,&data))==testState){data.lParam=oldState;SHAppBarMessage(ABM_SETSTATE,&data);}}}
};
// Compositor capture for the bounded real taskbar. GDI proof is retained as a
// comparison, since independent-flip/fullscreen applications can differ there.
int wmain(int argc,wchar_t** argv){
 if(argc!=2&&(argc!=3||wcscmp(argv[2],L"--expand-for-proof")))return 10;
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 try{
  winrt::init_apartment(winrt::apartment_type::multi_threaded);
  HWND taskbar=FindWindowW(L"Shell_TrayWnd",nullptr);RECT area{};AutoHideProofLease visibility;if(taskbar&&argc==3)visibility.expand(taskbar);
  if(!taskbar||!GetWindowRect(taskbar,&area)||!IsWindowVisible(taskbar))return 1;
  auto monitor=MonitorFromWindow(taskbar,MONITOR_DEFAULTTONEAREST);MONITORINFO mi{sizeof(mi)};if(!GetMonitorInfoW(monitor,&mi))return 2;
  RECT clipped{};if(!IntersectRect(&clipped,&area,&mi.rcMonitor))return 3;
  int w=clipped.right-clipped.left,h=clipped.bottom-clipped.top;if(w<1||w>8192||h<1||h>512)return 4;
  winrt::com_ptr<ID3D11Device> device;winrt::com_ptr<ID3D11DeviceContext> context;
  winrt::check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,device.put(),nullptr,context.put()));
  winrt::Windows::Foundation::IInspectable inspectable{nullptr};winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(device.as<IDXGIDevice>().get(),reinterpret_cast<IInspectable**>(winrt::put_abi(inspectable))));
  auto interop=winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem,IGraphicsCaptureItemInterop>();
  winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{nullptr};winrt::check_hresult(interop->CreateForMonitor(monitor,winrt::guid_of<decltype(item)>(),winrt::put_abi(item)));
  auto pool=winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>(),winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,2,item.Size());
  auto session=pool.CreateCaptureSession(item);session.IsCursorCaptureEnabled(false);session.StartCapture();
  winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame frame{nullptr};for(int i=0;i<200&&!frame;++i){frame=pool.TryGetNextFrame();if(!frame)Sleep(10);}if(!frame)return 5;
  winrt::com_ptr<TextureAccess> access;GUID accessId{0xa9b3d012,0x3df2,0x4ee3,{0xb8,0xd1,0x86,0x95,0xf4,0x57,0xd3,0xc1}};
  winrt::check_hresult(winrt::get_unknown(frame.Surface())->QueryInterface(accessId,access.put_void()));winrt::com_ptr<ID3D11Texture2D> texture;winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D),texture.put_void()));
  D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);int x=clipped.left-mi.rcMonitor.left,y=clipped.top-mi.rcMonitor.top;if(x<0||y<0||x+w>int(desc.Width)||y+h>int(desc.Height))return 6;
  desc.Width=w;desc.Height=h;desc.MipLevels=1;desc.ArraySize=1;desc.SampleDesc={1,0};desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
  winrt::com_ptr<ID3D11Texture2D> copy;winrt::check_hresult(device->CreateTexture2D(&desc,nullptr,copy.put()));D3D11_BOX box{UINT(x),UINT(y),0,UINT(x+w),UINT(y+h),1};context->CopySubresourceRegion(copy.get(),0,0,0,0,texture.get(),0,&box);
  D3D11_MAPPED_SUBRESOURCE mapped{};winrt::check_hresult(context->Map(copy.get(),0,D3D11_MAP_READ,0,&mapped));std::vector<unsigned char> pixels(size_t(w)*h*4);for(int row=0;row<h;++row)memcpy(pixels.data()+size_t(row)*w*4,reinterpret_cast<unsigned char*>(mapped.pData)+size_t(row)*mapped.RowPitch,w*4);context->Unmap(copy.get(),0);frame.Close();session.Close();pool.Close();
  BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=w;info.biHeight=-h;info.biPlanes=1;info.biBitCount=32;file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+DWORD(pixels.size());
  HANDLE output=CreateFileW(argv[1],GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(output==INVALID_HANDLE_VALUE)return 7;DWORD n=0;bool ok=WriteFile(output,&file,sizeof(file),&n,nullptr)&&WriteFile(output,&info,sizeof(info),&n,nullptr)&&WriteFile(output,pixels.data(),DWORD(pixels.size()),&n,nullptr)&&n==pixels.size();CloseHandle(output);printf("WGC taskbar proof %dx%d: %s\n",w,h,ok?"saved":"failed");return ok?0:8;
 }catch(winrt::hresult_error const& e){printf("WGC failure: 0x%08lx\n",static_cast<unsigned long>(e.code().value));return 9;}
}
