#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/audio.h"
#include "cinder/Log.h"

#include "cinder/CinderImGui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

constexpr auto sample27s = "file_example_MP3_700KB.mp3";

static int selectedAudioDevice = 2;

class AudioOutputsMacApp : public App {
public:
    void setup() override;
    void mouseDown(MouseEvent event) override;
    void mouseDrag(MouseEvent event) override;
    void keyDown(KeyEvent event) override;
    void resize() override;
    void update() override;
    void draw() override;

private:
    void loadAudio();
    void changeOutput();
    
    void refreshBuffer(size_t sampleRate);
    audio::BufferRef getBuffer(const char* file, size_t sampleRate);

    void seek(double seconds);
    void seekRelative(double seconds);
    void play();
    void pause();
    void stop();

    audio::Context* mCtx;
    shared_ptr<audio::BufferPlayerNode> mBufferPlayerNode;
    shared_ptr<audio::GainNode> mGain;

    int mWidth = 320;
    int mHeight = 240;
    
    audio::BufferRef mBuffer;
};

void AudioOutputsMacApp::setup()
{
    const auto displaySize = getDisplay()->getSize();
    const auto windowSize = ivec2{ 1000, 600 };
    setWindowSize(windowSize);
    setWindowPos((displaySize - windowSize) / 2);

    ImGui::Initialize();

    loadAudio();
    
    //mVideo.openFile(getAssetPath("Big_Buck_Bunny_10s.mp4"));
}

void AudioOutputsMacApp::keyDown(KeyEvent event)
{

    switch (event.getCode())
    {
    case KeyEvent::KEY_SPACE:
        if (mBufferPlayerNode->isEnabled())
        {
            pause();
        }
        else
        {
            play();
        }
        break;
    case KeyEvent::KEY_s:
        stop();
        break;
    case KeyEvent::KEY_LEFT:
        seekRelative(-1.0);
        break;
    case KeyEvent::KEY_RIGHT:
        seekRelative(1.0);
        break;
    case KeyEvent::KEY_v:
        break;
    default:
        break;
    }
}

void AudioOutputsMacApp::mouseDown(MouseEvent event)
{
    if (mBufferPlayerNode) {
        mBufferPlayerNode->seek(mBufferPlayerNode->getNumFrames() * event.getX() / getWindowWidth());
    }
}

void AudioOutputsMacApp::mouseDrag(MouseEvent event)
{
    if (mBufferPlayerNode) {
        mBufferPlayerNode->seek(mBufferPlayerNode->getNumFrames() * event.getX() / getWindowWidth());
    }
}

void AudioOutputsMacApp::resize()
{
}

void AudioOutputsMacApp::update()
{
}

void AudioOutputsMacApp::draw()
{
    gl::clear(Color(0, 0, 0));

    {
        // draw seek line
        if (mBufferPlayerNode) {
            float relPosition = mBufferPlayerNode->getReadPositionTime() / mBufferPlayerNode->getNumSeconds() * getWindowWidth();
            gl::ScopedColor(Color(1.f, 1.f, 1.f));
            gl::drawLine({ relPosition, 0 }, { relPosition, getWindowHeight() });
        }
    }

    {
        ImGui::ScopedWindow window("Audio Controls");
    

        // see available output devices
        std::string label;
        auto availableDevices = audio::Device::getOutputDevices();
        for (auto& device : availableDevices) {
            label += device->getName() + '\0';
        }
        
        if (ImGui::Combo("Device", &selectedAudioDevice, label.c_str()))
        {
            changeOutput();

            CI_LOG_I("Loaded " << sample27s);
        }

        if (ImGui::Button("Play"))
            play();

        ImGui::SameLine();
        if (ImGui::Button("Pause"))
            pause();

        ImGui::SameLine();
        if (ImGui::Button("Stop"))
            stop();

        if (mBufferPlayerNode) {
            float readPosition = static_cast<float>(mBufferPlayerNode->getReadPositionTime());
            float numSeconds = static_cast<float>(mBufferPlayerNode->getNumSeconds());

            if (ImGui::SliderFloat("Time", &readPosition, 0.f, numSeconds))
            {
                seek(readPosition);
            }
        }
        
    }
}

void AudioOutputsMacApp::refreshBuffer(size_t sampleRate){
    mBuffer = getBuffer(sample27s, sampleRate);
    
    if (mBuffer->getNumFrames() > 1) {
        // -- output audio to device
        if (mBufferPlayerNode) {
            mBufferPlayerNode->disable();
        }
        
        mBufferPlayerNode = mCtx->makeNode(new audio::BufferPlayerNode(mBuffer));

        mBufferPlayerNode >> mGain >> mCtx->getOutput();
    }
}

audio::BufferRef AudioOutputsMacApp::getBuffer(const char* file, size_t sampleRate) {

    audio::BufferRef buffer;

    const auto sourceFile = audio::load(loadAsset(file), sampleRate);
    buffer = sourceFile->loadBuffer();

    return buffer;
}

void AudioOutputsMacApp::loadAudio()
{
    // -- setup audio context
    mCtx = audio::Context::master();
    
    mGain = mCtx->makeNode(new audio::GainNode(1.f));

    mCtx->disable();
    
    const auto defaultOutput = mCtx->deviceManager()->getDefaultOutput();

    CI_LOG_I("Music output: " << defaultOutput->getName());
    CI_LOG_I("Output sample rate: " << defaultOutput->getSampleRate());

    if (defaultOutput->getSampleRate() > 10000) {
        refreshBuffer(defaultOutput->getSampleRate());
        
        play();
    }
}

void AudioOutputsMacApp::changeOutput() {

    auto availableDevices = audio::Device::getOutputDevices();
    
    audio::DeviceRef outDevice = availableDevices[selectedAudioDevice];
    
    if (outDevice->getSampleRate() == 0) {
        throw Exception("sample rate 0");
    }
    
    mCtx->disconnectAllNodes();
    mCtx->disable();
    
    auto outNode = mCtx->createOutputDeviceNode(outDevice);
    mCtx->setOutput(outNode);
    
    if (!mBufferPlayerNode || mBufferPlayerNode->getSampleRate() != outNode->getOutputSampleRate()) {
        refreshBuffer(outNode->getOutputSampleRate());
    }
    
    mBufferPlayerNode >> mGain >> mCtx->getOutput();
    
    play();
}

void AudioOutputsMacApp::pause()
{
    CI_LOG_V("pause");
    mBufferPlayerNode->disable();
}

void AudioOutputsMacApp::play()
{
    CI_LOG_V("play");
    mCtx->enable();
    mBufferPlayerNode->enable();
}

void AudioOutputsMacApp::stop()
{
    CI_LOG_V("stop");
    mBufferPlayerNode->stop();
}

void AudioOutputsMacApp::seek(double seconds)
{
    if (mBufferPlayerNode) {
        CI_LOG_V("seek to " << seconds);

        mBufferPlayerNode->seekToTime(seconds);
    }
}

void AudioOutputsMacApp::seekRelative(double seconds)
{

    double curr = mBufferPlayerNode->getReadPositionTime();
    CI_LOG_V("seek to " << curr + seconds);

    mBufferPlayerNode->seekToTime(curr + seconds);
}

CINDER_APP(  AudioOutputsMacApp, RendererGl, []( AudioOutputsMacApp::Settings *settings) {
    settings->setHighDensityDisplayEnabled(true);
})
