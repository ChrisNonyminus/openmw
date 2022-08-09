#include "sdlcursormanager.hpp"

#include <stdexcept>

#include <SDL_mouse.h>
#include <SDL_endian.h>
#include <SDL_render.h>
#include <SDL_hints.h>

#include <osg/GraphicsContext>
#include <osg/Geometry>
#include <osg/Texture2D>
#include <osg/TexMat>
#include <osg/Version>
#include <osgViewer/GraphicsWindow>

#include <components/debug/debuglog.hpp>

#include "imagetosurface.hpp"

#if defined(OSG_LIBRARY_STATIC) && (!defined(ANDROID) || OSG_VERSION_GREATER_THAN(3, 6, 5))
// Sets the default windowing system interface according to the OS.
// Necessary for OpenSceneGraph to do some things, like decompression.
USE_GRAPHICSWINDOW()
#endif

namespace CursorDecompression
{
    class MyGraphicsContext {
        public:
            MyGraphicsContext(int w, int h)
            {
                osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
                traits->x = 0;
                traits->y = 0;
                traits->width = w;
                traits->height = h;
                traits->red = 8;
                traits->green = 8;
                traits->blue = 8;
                traits->alpha = 8;
                traits->windowDecoration = false;
                traits->doubleBuffer = false;
                traits->sharedContext = nullptr;
                traits->pbuffer = true;

                osg::GraphicsContext::ScreenIdentifier si;
                si.readDISPLAY();
                if (si.displayNum<0) si.displayNum = 0;
                traits->displayNum = si.displayNum;
                traits->screenNum = si.screenNum;
                traits->hostName = si.hostName;

                _gc = osg::GraphicsContext::createGraphicsContext(traits.get());

                if (!_gc)
                {
                    Log(Debug::Warning) << "Failed to create pbuffer, failing back to normal graphics window.";

                    traits->pbuffer = false;
                    _gc = osg::GraphicsContext::createGraphicsContext(traits.get());
                    if (!_gc)
                        throw std::runtime_error("Failed to create graphics context for image decompression");
                }

                if (_gc.valid())
                {
                    _gc->realize();
                    _gc->makeCurrent();
                }
            }

            osg::ref_ptr<osg::GraphicsContext> getContext()
            {
                return _gc;
            }

            bool valid() const { return _gc.valid() && _gc->isRealized(); }

        private:
            osg::ref_ptr<osg::GraphicsContext> _gc;
    };

    SDLUtil::SurfaceUniquePtr softwareDecompress (osg::ref_ptr<osg::Image> source, float rotDegrees)
    {
        int width = source->s();
        int height = source->t();
        bool useAlpha = source->isImageTranslucent();

        osg::ref_ptr<osg::Image> decompressedImage = new osg::Image;
        decompressedImage->setFileName(source->getFileName());
        decompressedImage->allocateImage(width, height, 1, useAlpha ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE);
        for (int s=0; s<width; ++s)
            for (int t=0; t<height; ++t)
                decompressedImage->setColor(source->getColor(s,t,0), s,t,0);

        Uint32 redMask = 0x000000ff;
        Uint32 greenMask = 0x0000ff00;
        Uint32 blueMask = 0x00ff0000;
        Uint32 alphaMask = useAlpha ? 0xff000000 : 0;

        SDL_Surface *cursorSurface = SDL_CreateRGBSurfaceFrom(decompressedImage->data(),
            width,
            height,
            decompressedImage->getPixelSizeInBits(),
            decompressedImage->getRowSizeInBytes(),
            redMask,
            greenMask,
            blueMask,
            alphaMask);

        SDL_Surface *targetSurface = SDL_CreateRGBSurface(0, width, height, 32, redMask, greenMask, blueMask, alphaMask);
        SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(targetSurface);

        SDL_RenderClear(renderer);

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        SDL_Texture *cursorTexture = SDL_CreateTextureFromSurface(renderer, cursorSurface);

        SDL_RenderCopyEx(renderer, cursorTexture, nullptr, nullptr, -rotDegrees, nullptr, SDL_FLIP_VERTICAL);

        SDL_DestroyTexture(cursorTexture);
        SDL_FreeSurface(cursorSurface);
        SDL_DestroyRenderer(renderer);

        return SDLUtil::SurfaceUniquePtr(targetSurface, SDL_FreeSurface);
    }

}

namespace SDLUtil
{

    SDLCursorManager::SDLCursorManager() :
        mEnabled(false),
        mInitialized(false)
    {
    }

    SDLCursorManager::~SDLCursorManager()
    {
        CursorMap::const_iterator curs_iter = mCursorMap.begin();

        while(curs_iter != mCursorMap.end())
        {
            SDL_FreeCursor(curs_iter->second);
            ++curs_iter;
        }

        mCursorMap.clear();
    }

    void SDLCursorManager::setEnabled(bool enabled)
    {
        if(mInitialized && enabled == mEnabled)
            return;

        mInitialized = true;
        mEnabled = enabled;

        //turn on hardware cursors
        if(enabled)
        {
            _setGUICursor(mCurrentCursor);
        }
        //turn off hardware cursors
        else
        {
            SDL_ShowCursor(SDL_FALSE);
        }
    }

    void SDLCursorManager::cursorChanged(const std::string& name)
    {
        mCurrentCursor = name;
        _setGUICursor(name);
    }

    void SDLCursorManager::_setGUICursor(const std::string &name)
    {
        auto it = mCursorMap.find(name);
        if (it != mCursorMap.end())
            SDL_SetCursor(it->second);
    }

    void SDLCursorManager::createCursor(const std::string& name, int rotDegrees, osg::Image* image, Uint8 hotspot_x, Uint8 hotspot_y)
    {
#ifndef ANDROID
        _createCursorFromResource(name, rotDegrees, image, hotspot_x, hotspot_y);
#endif
    }

    void SDLCursorManager::_createCursorFromResource(const std::string& name, int rotDegrees, osg::Image* image, Uint8 hotspot_x, Uint8 hotspot_y)
    {
        if (mCursorMap.find(name) != mCursorMap.end())
            return;

        try {
            auto surface = CursorDecompression::softwareDecompress(image, static_cast<float>(rotDegrees));

            //set the cursor and store it for later
            SDL_Cursor* curs = SDL_CreateColorCursor(surface.get(), hotspot_x, hotspot_y);

            mCursorMap.insert(CursorMap::value_type(std::string(name), curs));
        } catch (std::exception& e) {
            Log(Debug::Warning) << e.what();
            Log(Debug::Warning) << "Using default cursor.";
            return;
        }
    }

}
