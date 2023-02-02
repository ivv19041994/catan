#define _USE_MATH_DEFINES 
#include "svg.h"


#include <cmath>
#include <iomanip>
#include <numbers>
#include <array>
#include <iostream>

namespace svg {

using namespace std::literals;
    
std::ostream& operator<<(std::ostream& os, const Point& point) {
    auto save = os.precision();
    
    //только 6 цфр, а не 6 после запятой, злой тренажер ругается за такую точность
    return os 
        << std::setprecision(6 /* + 1 + (int)log10(point.x)*/) << point.x << ","sv
        << std::setprecision(6 /* + 1 + (int)log10(point.y)*/) << point.y << std::setprecision(save);
}
std::ostream& operator<<(std::ostream& os, StrokeLineCap stroke_line_cap) {
    switch(stroke_line_cap) {
        case StrokeLineCap::BUTT: return os << "butt"sv;
        case StrokeLineCap::ROUND: return os << "round"sv;
        case StrokeLineCap::SQUARE: return os << "square"sv;
    }
    return os;
}
std::ostream& operator<<(std::ostream& os, StrokeLineJoin stroke_line_join) {
    switch(stroke_line_join) {
        case StrokeLineJoin::ARCS: return os << "arcs"sv;
        case StrokeLineJoin::BEVEL: return os << "bevel"sv;
        case StrokeLineJoin::MITER: return os << "miter"sv;
        case StrokeLineJoin::MITER_CLIP: return os << "miter-clip"sv;
        case StrokeLineJoin::ROUND: return os << "round"sv;
    }
    return os;
}
    
// Добавляет в svg-документ объект-наследник svg::Object
void Document::AddPtr(std::unique_ptr<Object>&& obj) {
    objects_.push_back(std::move(obj));
}
    
void Document::Render(std::ostream& out) const {
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"sv << std::endl;
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\">"sv << std::endl;

    RenderContext ctx(out, 2, 2);
    for(auto& obj: objects_) {
        obj->Render(ctx);
    }
    out << "</svg>"sv;
}


void Object::Render(const RenderContext& context) const {
    context.RenderIndent();

    // Делегируем вывод тега своим подклассам
    RenderObject(context);

    context.out << std::endl;
}

// ---------- Circle ------------------

    
Circle& Circle::SetCenter(Point center)  {
    center_ = center;
    return *this;
}

Circle& Circle::SetRadius(double radius)  {
    radius_ = radius;
    return *this;
}

void Circle::RenderObject(const RenderContext& context) const {
    auto& out = context.out;
    out << "<circle cx=\""sv << center_.x << "\" cy=\""sv << center_.y << "\" "sv;
    out << "r=\""sv << radius_ << "\""sv;
    RenderAttrs(out);
    out << "/>"sv;
}
    
Polyline& Polyline::AddPoint(Point point) {
    points_.push_back(std::move(point));
    return *this;
}
    
void Polyline::RenderObject(const RenderContext& context) const {
    auto& out = context.out;
    out << "<polyline points=\""sv;
    
    if(points_.size()) {
        out << points_[0];
        for(size_t i = 1; i < points_.size(); ++i) {
            out << " " << points_[i];
        }
    } 
    out << "\""sv;
    RenderAttrs(out);
    out << "/>"sv;
}
    
Text& Text::SetPosition(Point pos) {
    pos_ = pos;
    return *this;
}

    // Задаёт смещение относительно опорной точки (атрибуты dx, dy)
Text& Text::SetOffset(Point offset) {
    offset_ = offset;
    return *this;
}

Text& Text::SetFontSize(uint32_t size) {
    size_ = size;
    return *this;
}

    // Задаёт название шрифта (атрибут font-family)
Text& Text::SetFontFamily(std::string font_family) {
    font_family_ = font_family;
    return *this;
}

    // Задаёт толщину шрифта (атрибут font-weight)
Text& Text::SetFontWeight(std::string font_weight) {
    font_weight_ = font_weight;
    return *this;
}

    // Задаёт текстовое содержимое объекта (отображается внутри тега text)
Text& Text::SetData(std::string data) {
    data_ = data;
    return *this;
}
    
std::string Text::ShieldingString(const std::string& src) const {
    std::string ret;
    ret.reserve(src.size() * 2);
    for(char c: src) {
        switch(c) {
            case '\"':
                ret += "&quot;";
                break;
            case '\'':
                ret += "&apos;";
                break;
            case '<':
                ret += "&lt;";
                break;
             case '>':
                ret += "&gt;";
                break;
            case '&':
                ret += "&amp;";
                break;
            default:
                ret.push_back(c);
                break;
        }
    }
    return ret;
}
    
void Text::RenderObject(const RenderContext& context) const {
    auto& out = context.out;
    out << "<text"sv;
    
    RenderAttrs(out);
    
    out << " x=\"" << pos_.x << "\"";
    out << " y=\"" << pos_.y << "\"";   
        
    out << " dx=\"" << offset_.x << "\"";
    out << " dy=\"" << offset_.y << "\"";  
    
    out << " font-size=\"" << size_ << "\"";  

    if(font_family_.size()) {
        out << " font-family=\"" << font_family_ << "\"";
    }
    
    if(font_weight_.size()) {
        out << " font-weight=\"" << font_weight_ << "\"";
    }
    
    out << ">";
    out << ShieldingString(data_);
    out << "</text>";
}

}  // namespace svg

namespace shapes {

Triangle::Triangle(svg::Point p1, svg::Point p2, svg::Point p3)
    : p1_(p1)
    , p2_(p2)
    , p3_(p3) {
}

// Реализует метод Draw интерфейса svg::Drawable
void Triangle::Draw(svg::ObjectContainer& container) const {
    container.Add(svg::Polyline().AddPoint(p1_).AddPoint(p2_).AddPoint(p3_).AddPoint(p1_));
}

Star::Star(svg::Point center, double outer_radius, double inner_radius, int num_rays)
    : center_{center}
    , outer_radius_{outer_radius}
    , inner_radius_{inner_radius}
    , num_rays_{num_rays} {
}
    
svg::Polyline Star::CreateStar(svg::Point center, double outer_rad, double inner_rad, int num_rays) const {
    using namespace svg;
    Polyline polyline;
    for (int i = 0; i <= num_rays; ++i) {
        double angle = 2 * M_PI * (i % num_rays) / num_rays;
        polyline.AddPoint({center.x + outer_rad * sin(angle), center.y - outer_rad * cos(angle)});
        if (i == num_rays) {
            break;
        }
        angle += M_PI / num_rays;
        polyline.AddPoint({center.x + inner_rad * sin(angle), center.y - inner_rad * cos(angle)});
    }
    return polyline;
}

void Star::Draw(svg::ObjectContainer& container) const {
    container.Add(CreateStar(center_, outer_radius_, inner_radius_, num_rays_).SetFillColor("red").SetStrokeColor("black"));
}

Snowman::Snowman(svg::Point head_center, double head_radius)
    : head_center_{head_center}
    , head_radius_{head_radius} {
    
}

void Snowman::Draw(svg::ObjectContainer& container) const {
    container.Add(svg::Circle().SetCenter({head_center_.x, head_center_.y + head_radius_ * 5}).SetRadius(head_radius_ * 2.0).SetFillColor("rgb(240,240,240)").SetStrokeColor("black"));
    container.Add(svg::Circle().SetCenter({head_center_.x, head_center_.y + head_radius_ * 2}).SetRadius(head_radius_ * 1.5).SetFillColor("rgb(240,240,240)").SetStrokeColor("black"));
    container.Add(svg::Circle().SetCenter({head_center_.x, head_center_.y}).SetRadius(head_radius_).SetFillColor("rgb(240,240,240)").SetStrokeColor("black"));
}

Hexagon::Hexagon(svg::Point center, double edge, double angle)
    : center_{ center }
    , edge_{ edge }
    , angle_{ angle } {

}
void Hexagon::Draw(svg::ObjectContainer& container) const {
    auto hexagon = svg::Polyline();

    if (color_) {
        hexagon.SetFillColor(*color_);
    }

    constexpr std::array<double, 7> angles{0, 60, 120, 180, 240, 300, 360 };

    for (double angle : angles) {
        double a = (angle / 180.0) * std::numbers::pi;
        hexagon.AddPoint({ std::cos(a - angle_) * edge_ + center_.x, std::sin(a - angle_) * edge_ + center_.y});
    }

    container.Add(std::move(hexagon));
}

Hexagon& Hexagon::SetColor(svg::Color color) {
    color_ = std::move(color);
    return *this;
}



}//namespace shapes