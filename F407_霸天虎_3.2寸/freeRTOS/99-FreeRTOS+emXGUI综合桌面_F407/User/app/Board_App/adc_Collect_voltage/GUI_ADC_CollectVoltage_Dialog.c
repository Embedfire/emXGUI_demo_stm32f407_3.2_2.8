#include <emXGUI.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "ff.h"
#include "x_libc.h"
#include "GUI_AppDef.h"
#include "emXGUI_JPEG.h"
#include "emxgui_png.h"
#include  "GUI_ADC_CollectVoltage_Dialog.h"
#include "bsp_adc.h"


/* 窗口 ID */
#define ID_ADV_WIN         0x01    // 中间显示窗口ID
#define SCROLLBAR_Brigh_ID 0x02    // 滑动条的ID
#define ID_TEXTBOX_Title   0x03    // 标题栏
#define ID_TEXTBOX_Brigh   0x04    // 亮度百分比

#define CircleCenter_1    (46)     // 三角形旋转半径
#define CircleCenter_2    (64)    // 圆弧进度条半径（小）
#define CircleCenter_3    (CircleCenter_2 + 6)    //  不大于 CircleSize / 2

/* 移动方向标志 */
#define LeftToRight    0
#define RightToLeft    1
#define MOVE_WIN       1

#define CircleSize    140    // 圆形显示区域的大小
#define Circle_X      169    // 圆形显示区域的位置
#define Circle_Y      (54)   // 圆形显示区域的位置

#define GUI_ADC_BACKGROUNG_PIC      "adc_desktop.jpg"

#define TitleHeight    28    // 标题栏的高度

#define TriangleLen    10    // 三角形的边长

uint8_t AovingDirection = 0;
//double count = 0.0;
HWND MAIN_Handle;
HWND Title_Handle;
HWND Brigh_Handle;
HWND ADC_Handle;

HDC bk_hdc, TrianglePointer_DC;

static BITMAP bm_Triangle;

// 局部变量，用于保存转换计算后的电压值 	 
double ADC_Vol; 

static void	X_MeterPointer(HDC hdc, int cx, int cy, int r, u32 color, double dat_val)
{
  double angle;
  int midpoint_x,midpoint_y;
  POINT pt[4];

  angle = (dat_val * 1.427 );    // 计算角度 + 0.785 = 45°
  
  /* 计算与切线平行的一边的中点坐标 */
  midpoint_x =cx - sin(angle) * (r - TriangleLen * 0.866);    // 0.866 = sqrt(3) / 2
  midpoint_y =cy + cos(angle) * (r - TriangleLen * 0.866);

  /* 最远的一个点 */
  pt[0].x = cx - r * sin(angle);
  pt[0].y = cy + r * cos(angle);

  /* 最远一点的左边一点 */
  pt[1].x = midpoint_x - (TriangleLen / 2) * sin(angle - 1.57);    // 1.57 = 3.14/2 = π/2 = 90°
  pt[1].y = midpoint_y + (TriangleLen / 2) * cos(angle - 1.57);

  /* 最远一点的右边一点 */
  pt[2].x = midpoint_x - (TriangleLen / 2) * sin(angle + 1.57);
  pt[2].y = midpoint_y + (TriangleLen / 2) * cos(angle + 1.57);

  pt[3].x = pt[0].x;
  pt[3].y = pt[0].y;


  /* 画三角形 */
  SetBrushColor(hdc,color);
  EnableAntiAlias(hdc, TRUE);
  FillPolygon(hdc,0,0,pt,4);
  EnableAntiAlias(hdc, FALSE);
}

//退出按钮重绘制
static void CollectVoltage_ExitButton_OwnerDraw(DRAWITEM_HDR *ds)
{
  HDC hdc;
  RECT rc;

	hdc = ds->hDC;   
	rc = ds->rc; 

  if (ds->State & BST_PUSHED)
	{ //按钮是按下状态
		SetPenColor(hdc, MapRGB(hdc, 250, 250, 250));
	}
	else
	{ //按钮是弹起状态

		SetPenColor(hdc, MapRGB(hdc, 1, 191, 255));      //设置画笔色
	}
  
  for(int i=0; i<4; i++)
  {
    HLine(hdc, rc.x, rc.y, rc.w);
    rc.y += 5;
  }
}

/*
 * @brief  重绘透明文本
 * @param  ds:	自定义绘制结构体
 * @retval NONE
*/
static void Textbox_OwnerDraw(DRAWITEM_HDR *ds) //绘制一个按钮外观
{
	HWND hwnd;
	HDC hdc;
	RECT rc, rc_tmp;
	WCHAR wbuf[128];

	hwnd = ds->hwnd; //button的窗口句柄.
	hdc = ds->hDC;   //button的绘图上下文句柄.
  GetClientRect(hwnd, &rc_tmp);//得到控件的位置
  GetClientRect(hwnd, &rc);//得到控件的位置
  WindowToScreen(hwnd, (POINT *)&rc_tmp, 1);//坐标转换

  BitBlt(hdc, rc.x, rc.y, rc.w, rc.h, bk_hdc, rc_tmp.x, rc_tmp.y, SRCCOPY);
  SetTextColor(hdc, MapRGB(hdc, 255, 255, 255));

  GetWindowText(hwnd, wbuf, 128); //获得按钮控件的文字

  DrawText(hdc, wbuf, -1, &rc, DT_VCENTER|DT_CENTER);//绘制文字(居中对齐方式)
}

/* 重绘圆形显示区域 */
void Circle_Paint(HWND hwnd, HDC hdc)
{
  char  cbuf[128];
  WCHAR wbuf[128];
  int vertex_x,vertex_y;
  RECT rc = {0, 0, CircleSize, CircleSize};
  int angle = ((225 - (-45))) * ADC_Vol / 3.3;

  EnableAntiAlias(hdc, TRUE);

  RotateBitmap(hdc, CircleSize/2, CircleSize/2, &bm_Triangle, angle + 45);
  
  if (ADC_Vol > 0.03)    // 电压太小不画白色小圆圈
  {
    SetBrushColor(hdc, MapARGB(hdc, 255, 200, 200, 200));
    FillArc(hdc, CircleSize/2, CircleSize/2 + 2, CircleCenter_2+1, CircleCenter_3-1, -45, angle - 45);
    /* 计算左边小圆圈的坐标 */
    vertex_x =  CircleSize / 2 - ((CircleCenter_3 - CircleCenter_2) / 2 + CircleCenter_2) * sin(3.14/4); 
    vertex_y =  CircleSize / 2 + ((CircleCenter_3 - CircleCenter_2) / 2 + CircleCenter_2) * cos(3.14/4) + 2; 
    FillCircle(hdc, vertex_x, vertex_y, (CircleCenter_3 - CircleCenter_2) / 2 - 1);     // 左边小圆圈
  }

  EnableAntiAlias(hdc, FALSE);

  /* 使用默认字体 */
	SetFont(hdc, defaultFont);

  rc.w = 24*3;
  rc.h = 48;
  rc.x = CircleSize/2 - rc.w/2 - 5;
  rc.y = CircleSize/2 - rc.h/2;

  /* 显示电压百分比 */
  SetTextColor(hdc, MapARGB(hdc, 255, 0, 0, 0));
  SetFont(hdc, controlFont_32);
  x_sprintf(cbuf, "%d", (int)(ADC_Vol/3.3*100));    // H -> % x_wsprintf(Backlightwbuf, L"%d", i);
  x_mbstowcs(wbuf, cbuf, 128);
  rc.w -= 20;
  DrawText(hdc, wbuf, -1, &rc, DT_VCENTER|DT_RIGHT);    // 绘制文字(居中对齐方式)DT_CENTER
  rc.w += 20;
  rc.y += 4;
  SetFont(hdc, controlFont_24);
  DrawText(hdc, L"H", -1, &rc, DT_VCENTER|DT_RIGHT);    // 绘制文字(居中对齐方式)DT_CENTER

  /* 显示采集到的电压值 */
  SetFont(hdc, defaultFont);
  rc.w = 24*4 - 10;
  rc.h = 30;
  rc.y = CircleSize/2 - rc.h/2 + CircleCenter_3 - 8;
  x_sprintf(cbuf, "%.2fV", ADC_Vol);
  x_mbstowcs(wbuf, cbuf, 128);
  DrawText(hdc, wbuf, -1, &rc, DT_VCENTER|DT_CENTER);    // 绘制文字(居中对齐方式)
}
 
static LRESULT	CollectVoltage_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
    case WM_CREATE:
    {
      RECT rc;
      GetClientRect(hwnd, &rc); 
      ADCx_Init();    // 初始化 ADC
            
      CreateWindow(BUTTON, L"O", WS_TRANSPARENT|BS_FLAT | BS_NOTIFY |WS_OWNERDRAW|WS_VISIBLE,
                  286, 10, 25, 25, hwnd, eID_ADC_EXIT, NULL, NULL); 

      rc.w = GUI_XSIZE / 2;
      rc.h = TitleHeight;
      rc.x = GUI_XSIZE / 2 - rc.w / 2;
      rc.y = 0;

      Title_Handle = CreateWindow(TEXTBOX, L"ADC一电位器电压显示", WS_VISIBLE | WS_OWNERDRAW, rc.x, rc.y, rc.w, rc.h, hwnd, ID_TEXTBOX_Title, NULL, NULL);//
      SendMessage(Title_Handle, TBM_SET_TEXTFLAG, 0, DT_VCENTER | DT_CENTER | DT_BKGND);   

       /* 画三角形指针 */
      TrianglePointer_DC = CreateMemoryDC((SURF_FORMAT)COLOR_FORMAT_ARGB8888, TriangleLen, CircleCenter_1 * 2);    // 创建三角形指针内存 DC
      ClrDisplay(TrianglePointer_DC, NULL, 0);
      X_MeterPointer(TrianglePointer_DC, TriangleLen/2, CircleCenter_1, CircleCenter_1-2, MapARGB(TrianglePointer_DC, 255, 250, 20, 20), 0);
      /* 转换成bitmap */
      DCtoBitmap(TrianglePointer_DC,&bm_Triangle);

      BOOL res;
      u8 *jpeg_buf;
      u32 jpeg_size;
      JPG_DEC *dec;

      if (strstr(GUI_ADC_BACKGROUNG_PIC, "0:/") == NULL)
        res = RES_Load_Content(GUI_ADC_BACKGROUNG_PIC, (char**)&jpeg_buf, &jpeg_size);
      else
        res = FS_Load_Content(GUI_ADC_BACKGROUNG_PIC, (char**)&jpeg_buf, &jpeg_size);

      bk_hdc = CreateMemoryDC(SURF_SCREEN, GUI_XSIZE, GUI_YSIZE);
      if(res)
      {
        /* 根据图片数据创建JPG_DEC句柄 */
        dec = JPG_Open(jpeg_buf, jpeg_size);

        /* 绘制至内存对象 */
        JPG_Draw(bk_hdc, 0, 0, dec);

        /* 关闭JPG_DEC句柄 */
        JPG_Close(dec);
      }
      /* 释放图片内容空间 */
      RES_Release_Content((char **)&jpeg_buf);

      SetTimer(hwnd, 2, 20, TMR_START, NULL);

      break;
    } 
    case WM_TIMER:
    {
      int tmr_id = wParam;

      if (tmr_id == 2)
      {
        RECT rc;
        static double ADC_buff = 0.0;
        double vol_buff = 0.0;
        static uint8_t xC = 0;

        vol_buff =(double) ADC_ConvertedValue/4096*(double)3.3; // 读取转换的AD值
//        GUI_DEBUG("电压值前为：%f", ADC_Vol);
        #if 1

          if (xC++ < 10)
          {
            ADC_buff += vol_buff;
            break;
          }
          else
          {
            ADC_Vol = ADC_buff / ((double)(xC-1));
            ADC_buff = 0;
            xC = 0;
          }

        #else
          
        ADC_Vol = (double)(((int)(vol_buff * 10)) / 10.0);

        #endif
//        GUI_DEBUG("电压值后为：%f", ADC_Vol);
        
        rc.x = Circle_X;
        rc.y = Circle_Y;
        rc.w = CircleSize * 2;
        rc.h = CircleSize * 2;

        InvalidateRect(hwnd, &rc, FALSE);
      }
      break;
    }
    case WM_ERASEBKGND:
    {
      
      HDC hdc =(HDC)wParam;
      RECT rc =*(RECT*)lParam;
      
      BitBlt(hdc, rc.x, rc.y, rc.w, rc.h, bk_hdc, rc.x, rc.y, SRCCOPY);

      return TRUE;
    }

    case WM_PAINT:
    {
      HDC hdc , hdc_mem;
      PAINTSTRUCT ps;
      //  RECT rc = {0,0,800,70};
      //  hdc_mem = CreateMemoryDC(SURF_ARGB4444, 800,70);

      hdc_mem = CreateMemoryDC(SURF_SCREEN, CircleSize, CircleSize);

      hdc = BeginPaint(hwnd, &ps);

      BitBlt(hdc_mem, 0, 0, CircleSize, CircleSize, bk_hdc, Circle_X, Circle_Y, SRCCOPY);

      Circle_Paint(hwnd, hdc_mem);    // 绘制第一个界面中的圆形显示区域
      
      BitBlt(hdc, Circle_X, Circle_Y, CircleSize, CircleSize, hdc_mem, 0, 0, SRCCOPY);
      DeleteDC(hdc_mem);

      EndPaint(hwnd, &ps);

      break;
    } 
    case WM_DRAWITEM:
    {
       DRAWITEM_HDR *ds;
       ds = (DRAWITEM_HDR*)lParam;
       switch(ds->ID)
       {
          case eID_ADC_EXIT:
          {
            CollectVoltage_ExitButton_OwnerDraw(ds);
            return TRUE;             
          }    

          case ID_TEXTBOX_Title:
          {
            Textbox_OwnerDraw(ds);
            return TRUE;             
          } 
       }

       break;
    }
    case WM_NOTIFY:
    {
      u16 code, id;
      id  =LOWORD(wParam);//获取消息的ID码
      code=HIWORD(wParam);//获取消息的类型    
      if(code == BN_CLICKED && id == eID_ADC_EXIT)
      {
        PostCloseMessage(hwnd);
        break;
      }

      break;
    } 

    case WM_LBUTTONUP:
    {
      return	DefWindowProc(hwnd, msg, wParam, lParam);   
    }

    case WM_MOUSEMOVE:
    {
      return	DefWindowProc(hwnd, msg, wParam, lParam);
    }

    case WM_LBUTTONDOWN:
    {
      return	DefWindowProc(hwnd, msg, wParam, lParam);   
    }

    case WM_DESTROY:
    {
      stopADC();    // 停止ADC的采集
      DeleteDC(bk_hdc);
      DeleteDC(TrianglePointer_DC);
      return PostQuitMessage(hwnd);	
    } 

    default:
      return	DefWindowProc(hwnd, msg, wParam, lParam);   
  }
  
  return WM_NULL;
  
}

void GUI_ADC_CollectVoltage_Dialog(void)
{
	
	WNDCLASS	wcex;
	MSG msg;

	wcex.Tag = WNDCLASS_TAG;

	wcex.Style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = CollectVoltage_proc; //设置主窗口消息处理的回调函数.
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = NULL;//hInst;
	wcex.hIcon = NULL;//LoadIcon(hInstance, (LPCTSTR)IDI_WIN32_APP_TEST);
	wcex.hCursor = NULL;//LoadCursor(NULL, IDC_ARROW);
    
	//创建主窗口
	MAIN_Handle = CreateWindowEx(WS_EX_NOFOCUS|WS_EX_FRAMEBUFFER,
                              &wcex,
                              L"GUI_ADC_CollectVoltage_Dialog",
                              WS_VISIBLE|WS_CLIPCHILDREN,
                              0, 0, GUI_XSIZE, GUI_YSIZE,
                              NULL, NULL, NULL, NULL);
   //显示主窗口
	ShowWindow(MAIN_Handle, SW_SHOW);
	//开始窗口消息循环(窗口关闭并销毁时,GetMessage将返回FALSE,退出本消息循环)。
	while (GetMessage(&msg, MAIN_Handle))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}  
}


