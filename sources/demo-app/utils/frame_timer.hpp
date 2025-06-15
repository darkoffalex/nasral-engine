#pragma once

namespace utils
{
    using namespace std::chrono;

    class FrameTimer
    {
    public:
        FrameTimer()
        : on_fps_refreshed_(nullptr)
        , prev_frame_(steady_clock::now())
        , delta_(0.0f)
        , until_fps_refresh_(1.0f)
        , fps_(0)
        , last_fps_(0)
        {}

        ~FrameTimer() = default;

        void update()
        {
            // Delta
            const auto now = steady_clock::now();
            delta_ = std::chrono::duration<float>(now - prev_frame_).count();
            prev_frame_ = now;

            // FPS
            until_fps_refresh_ -= delta_;
            if(until_fps_refresh_ <= 0.0f)
            {
                last_fps_ = fps_;
                fps_ = 0;
                until_fps_refresh_ = 1.0f;

                if(on_fps_refreshed_)
                {
                    on_fps_refreshed_(last_fps_);
                }
            }

            fps_++;
        }

        [[nodiscard]] float delta() const
        {
            return delta_;
        }

        [[nodiscard]] unsigned last_fps() const
        {
            return last_fps_;
        }

        void set_fps_refresh_fn(const std::function<void(unsigned fps)>& fn)
        {
            on_fps_refreshed_ = fn;
        }

    protected:
        std::function<void(unsigned fps)>   on_fps_refreshed_;
        time_point<steady_clock>            prev_frame_;
        float                               delta_;
        float                               until_fps_refresh_;
        unsigned                            fps_;
        unsigned                            last_fps_;
    };
}