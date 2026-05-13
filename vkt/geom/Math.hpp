/**
 * @file Math.hpp
 * @brief VKT-CAD Mühendislik Matematik Motoru.
 * 
 * Bu dosya, CAD çekirdeği ve Vulkan grafik motoru için gerekli olan 
 * temel lineer cebir yapılarını (Vector3, Matrix4, Transformations) içerir.
 */
#pragma once
#include <cmath>
#include <array>
#include <algorithm>

namespace vkt {
namespace geom {

/** @brief Yüzer noktalı sayı karşılaştırmaları için tolerans değeri. */
constexpr double EPSILON = 1e-9;
/** @brief Pi sayısı. */
constexpr double PI = 3.14159265358979323846;
/** @brief CAD çizimlerinde iki noktanın aynı kabul edilmesi için maksimum mesafe (Metre). */
inline constexpr double CAD_TOLERANCE = 0.001; // 1mm hassasiyet

/**
 * @struct Vec3
 * @brief 3-Boyutlu Uzayda Vektör ve Nokta Tanımı.
 */
struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3() = default;
    Vec3(double _x, double _y, double _z) : x(_x), y(_y), z(_z) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
    
    /** @brief Vektörün uzunluğunu (norm) hesaplar. */
    double Length() const { return std::sqrt(x*x + y*y + z*z); }
    
    /** @brief Vektörü birim vektöre dönüştürür. */
    Vec3 Normalize() const {
        double l = Length();
        return (l > EPSILON) ? (*this * (1.0 / l)) : Vec3{0,0,0};
    }

    /** @brief İki vektör arası Öklid mesafesini hesaplar. */
    double DistanceTo(const Vec3& other) const {
        return (*this - other).Length();
    }

    bool operator==(const Vec3& other) const {
        return DistanceTo(other) < CAD_TOLERANCE;
    }

    /** @brief İki vektör arasındaki skaler çarpımı (Dot Product) hesaplar. */
    static double Dot(const Vec3& a, const Vec3& b) {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    /** @brief İki vektör arasındaki vektörel çarpımı (Cross Product) hesaplar. */
    static Vec3 Cross(const Vec3& a, const Vec3& b) {
        return {
            a.y*b.z - a.z*b.y,
            a.z*b.x - a.x*b.z,
            a.x*b.y - a.y*b.x
        };
    }
};

/** @brief İki nokta arasındaki Öklid mesafesini hesaplar (Mühendislik Hassasiyeti). */
inline double Distance(const Vec3& a, const Vec3& b) {
    return a.DistanceTo(b);
}

/**
 * @struct Ray
 * @brief 3D Uzayda Işın Tanımı (Seçim ve Yakalama İşlemleri İçin).
 */
struct Ray {
    Vec3 origin;    ///< Işının başlangıç noktası
    Vec3 direction; ///< Işının birim doğrultu vektörü

    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(d.Normalize()) {}

    /** @brief Işın üzerinde t parametresine karşılık gelen noktayı döner. */
    Vec3 PointAt(double t) const { return origin + (direction * t); }

    /** @brief Sonsuz düzlemle kesişim: plane_normal·(P - plane_point) = 0. */
    bool IntersectsPlane(const Vec3& normal, const Vec3& planePoint, double& t) const {
        double denom = Vec3::Dot(normal, direction);
        if (std::abs(denom) < EPSILON) return false;
        t = Vec3::Dot(normal, planePoint - origin) / denom;
        return t >= 0.0;
    }

    /** @brief pA–pB ekseni etrafında radius yarıçaplı silindirle kesişim. */
    bool IntersectsCylinder(const Vec3& pA, const Vec3& pB, double radius, double& t) const {
        Vec3 axis = (pB - pA).Normalize();
        Vec3 d = direction - axis * Vec3::Dot(direction, axis);
        Vec3 oc = origin - pA;
        Vec3 f = oc - axis * Vec3::Dot(oc, axis);

        double a = Vec3::Dot(d, d);
        double b = 2.0 * Vec3::Dot(f, d);
        double c = Vec3::Dot(f, f) - radius * radius;

        double disc = b * b - 4.0 * a * c;
        if (disc < 0.0 || a < EPSILON) return false;

        double sqrtDisc = std::sqrt(disc);
        double t0 = (-b - sqrtDisc) / (2.0 * a);
        double t1 = (-b + sqrtDisc) / (2.0 * a);
        t = (t0 >= 0.0) ? t0 : t1;
        if (t < 0.0) return false;

        // Silindir uzunluk sınırı kontrolü
        Vec3 hitPoint = PointAt(t);
        double proj = Vec3::Dot(hitPoint - pA, axis);
        double len = (pB - pA).Length();
        return proj >= 0.0 && proj <= len;
    }
};

/** @struct Vertex
 * @brief GPU'ya gönderilecek tepe noktası verisi.
 * Vulkan bellek düzenine (Binding) uyumludur.
 */
struct Vertex {
    float pos[3];    // x, y, z
    float normal[3]; // nx, ny, nz (Işıklandırma için)
    float color[3];  // r, g, b
};

/**
 * @struct Mat4
 * @brief 4x4 Homojen Transformasyon Matrisi.
 * 
 * Vulkan render pipeline ve 3D projeksiyon işlemleri için column-major formatındadır.
 */
struct Mat4 {
    std::array<float, 16> data{};

    Mat4() { Identity(); }

    /** @brief Birim matris (Identity Matrix) oluşturur. */
    void Identity() {
        data.fill(0.0f);
        data[0] = data[5] = data[10] = data[15] = 1.0f;
    }

    /** @brief Matris çarpımı (M1 * M2). */
    Mat4 operator*(const Mat4& o) const {
        Mat4 res;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                res.data[i + j * 4] = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    res.data[i + j * 4] += data[i + k * 4] * o.data[k + j * 4];
                }
            }
        }
        return res;
    }

    /** @brief Öteleme (Translation) matrisi oluşturur. */
    static Mat4 Translate(const Vec3& v) {
        Mat4 m;
        m.data[12] = static_cast<float>(v.x);
        m.data[13] = static_cast<float>(v.y);
        m.data[14] = static_cast<float>(v.z);
        return m;
    }

    /** 
     * @brief Matrisin tersini (Inverse) hesaplar. 
     * Picking (Seçim) işlemleri için NDC'den Dünya koordinatlarına dönüşte gereklidir.
     */
    Mat4 Inverse() const {
        Mat4 inv;
        float* m = const_cast<float*>(data.data());
        float* out = inv.data.data();

        float inv_s[16];
        inv_s[0] = m[5]  * m[10] * m[15] - m[5]  * m[11] * m[14] - m[9]  * m[6]  * m[15] + m[9]  * m[7]  * m[14] + m[13] * m[6]  * m[11] - m[13] * m[7]  * m[10];
        inv_s[4] = -m[4]  * m[10] * m[15] + m[4]  * m[11] * m[14] + m[8]  * m[6]  * m[15] - m[8]  * m[7]  * m[14] - m[12] * m[6]  * m[11] + m[12] * m[7]  * m[10];
        inv_s[8] = m[4]  * m[9]  * m[15] - m[4]  * m[11] * m[13] - m[8]  * m[5]  * m[15] + m[8]  * m[7]  * m[13] + m[12] * m[5]  * m[11] - m[12] * m[7]  * m[9];
        inv_s[12] = -m[4]  * m[9]  * m[14] + m[4]  * m[10] * m[13] + m[8]  * m[5]  * m[14] - m[8]  * m[6]  * m[13] - m[12] * m[5]  * m[10] + m[12] * m[6]  * m[9];
        inv_s[1] = -m[1]  * m[10] * m[15] + m[1]  * m[11] * m[14] + m[9]  * m[2]  * m[15] - m[9]  * m[3]  * m[14] - m[13] * m[2]  * m[11] + m[13] * m[3]  * m[10];
        inv_s[5] = m[0]  * m[10] * m[15] - m[0]  * m[11] * m[14] - m[8]  * m[2]  * m[15] + m[8]  * m[3]  * m[14] + m[12] * m[2]  * m[11] - m[12] * m[3]  * m[10];
        inv_s[9] = -m[0]  * m[9]  * m[15] + m[0]  * m[11] * m[13] + m[8]  * m[1]  * m[15] - m[8]  * m[3]  * m[13] - m[12] * m[1]  * m[11] + m[12] * m[3]  * m[9];
        inv_s[13] = m[0]  * m[9]  * m[14] - m[0]  * m[10] * m[13] - m[8]  * m[1]  * m[14] + m[8]  * m[2]  * m[13] + m[12] * m[1]  * m[10] - m[12] * m[2]  * m[9];
        inv_s[2] = m[1]  * m[6]  * m[15] - m[1]  * m[7]  * m[14] - m[5]  * m[2]  * m[15] + m[5]  * m[3]  * m[14] + m[13] * m[2]  * m[7]  - m[13] * m[3]  * m[6];
        inv_s[6] = -m[0]  * m[6]  * m[15] + m[0]  * m[7]  * m[14] + m[4]  * m[2]  * m[15] - m[4]  * m[3]  * m[14] - m[12] * m[2]  * m[7]  + m[12] * m[3]  * m[6];
        inv_s[10] = m[0]  * m[5]  * m[15] - m[0]  * m[7]  * m[13] - m[4]  * m[1]  * m[15] + m[4]  * m[3]  * m[13] + m[12] * m[1]  * m[7]  - m[12] * m[3]  * m[5];
        inv_s[14] = -m[0]  * m[5]  * m[14] + m[0]  * m[6]  * m[13] + m[4]  * m[1]  * m[14] - m[4]  * m[2]  * m[13] - m[12] * m[1]  * m[6]  + m[12] * m[2]  * m[5];
        inv_s[3] = -m[1]  * m[6]  * m[11] + m[1]  * m[7]  * m[10] + m[5]  * m[2]  * m[11] - m[5]  * m[3]  * m[10] - m[9]  * m[2]  * m[7]  + m[9]  * m[3]  * m[6];
        inv_s[7] = m[0]  * m[6]  * m[11] - m[0]  * m[7]  * m[10] - m[4]  * m[2]  * m[11] + m[4]  * m[3]  * m[10] + m[8]  * m[2]  * m[7]  - m[8]  * m[3]  * m[6];
        inv_s[11] = -m[0]  * m[5]  * m[11] + m[0]  * m[7]  * m[9]  + m[4]  * m[1]  * m[11] - m[4]  * m[3]  * m[9]  - m[8]  * m[1]  * m[7]  + m[8]  * m[3]  * m[5];
        inv_s[15] = m[0]  * m[5]  * m[10] - m[0]  * m[6]  * m[9]  - m[4]  * m[1]  * m[10] + m[4]  * m[2]  * m[9]  + m[8]  * m[1]  * m[6]  - m[8]  * m[2]  * m[5];

        float det = m[0] * inv_s[0] + m[1] * inv_s[4] + m[2] * inv_s[8] + m[3] * inv_s[12];
        if (std::abs(det) < EPSILON) return inv;

        det = 1.0f / det;
        for (int i = 0; i < 16; i++) out[i] = inv_s[i] * det;
        return inv;
    }

    /** @brief Bir noktayı matris ile çarpar (Transformasyon + Perspektif Bölme). */
    Vec3 Transform(const Vec3& v) const {
        float x = (float)v.x, y = (float)v.y, z = (float)v.z;
        float w = data[3] * x + data[7] * y + data[11] * z + data[15];
        if (std::abs(w) < (float)EPSILON) w = 1.0f;
        return {
            (data[0] * x + data[4] * y + data[8]  * z + data[12]) / w,
            (data[1] * x + data[5] * y + data[9]  * z + data[13]) / w,
            (data[2] * x + data[6] * y + data[10] * z + data[14]) / w
        };
    }

    /** @brief Bakış (LookAt) matrisi oluşturur. (Mühendislik 3D Görünümü için) */
    static Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).Normalize();
        Vec3 s = Vec3::Cross(f, up).Normalize();
        Vec3 u = Vec3::Cross(s, f);

        Mat4 m;
        m.data[0] = static_cast<float>(s.x);
        m.data[4] = static_cast<float>(s.y);
        m.data[8] = static_cast<float>(s.z);
        
        m.data[1] = static_cast<float>(u.x);
        m.data[5] = static_cast<float>(u.y);
        m.data[9] = static_cast<float>(u.z);
        
        m.data[2] = static_cast<float>(-f.x);
        m.data[6] = static_cast<float>(-f.y);
        m.data[10] = static_cast<float>(-f.z);

        m.data[12] = static_cast<float>(-Vec3::Dot(s, eye));
        m.data[13] = static_cast<float>(-Vec3::Dot(u, eye));
        m.data[14] = static_cast<float>(Vec3::Dot(f, eye));
        return m;
    }

    /** @brief Perspektif Projeksiyon matrisi oluşturur. */
    static Mat4 Perspective(float fov_rad, float aspect, float near_z, float far_z) {
        float tanHalfFov = std::tan(fov_rad / 2.0f);
        Mat4 m;
        m.data.fill(0.0f);
        m.data[0] = 1.0f / (aspect * tanHalfFov);
        m.data[5] = 1.0f / (tanHalfFov);
        m.data[10] = far_z / (near_z - far_z);
        m.data[11] = -1.0f;
        m.data[14] = -(far_z * near_z) / (far_z - near_z);
        return m;
    }
};

/**
 * @class Camera
 * @brief 3D Sahne Gezinti Motoru.
 * 
 * Orbit (yörünge) tipi kamera mantığı ile çalışır, 
 * tesisat projesine her açıdan bakmayı sağlar.
 */
class Camera {
public:
    Vec3 eye{10.0, 10.0, 10.0};    ///< Kameranın konumu
    Vec3 center{0.0, 0.0, 0.0};  ///< Kameranın baktığı odak noktası
    Vec3 up{0.0, 0.0, 1.0};      ///< Yukarı vektörü (Z-up standardı)

    float fov = 45.0f;
    float aspect = 1.0f;

    Mat4 GetViewMatrix() const { return Mat4::LookAt(eye, center, up); }
    Mat4 GetProjectionMatrix() const { return Mat4::Perspective(fov * (3.14159f / 180.0f), aspect, 0.1f, 1000.0f); }

    /** 
     * @brief Ekran koordinatlarını 3D Dünya ışınına çevirir (Object Picking / Selection).
     * @param screenX X koordinatı (0 - Width)
     * @param screenY Y koordinatı (0 - Height)
     * @param width Ekran genişliği
     * @param height Ekran yüksekliği
     */
    Ray ScreenPointToRay(float screenX, float screenY, float width, float height) const {
        // 1. Normalized Device Coordinates (NDC) [-1, 1]
        float x = (2.0f * screenX) / width - 1.0f;
        float y = 1.0f - (2.0f * screenY) / height; // Y inverted in screen space

        Mat4 view = GetViewMatrix();
        Mat4 proj = GetProjectionMatrix();
        Mat4 invVP = (proj * view).Inverse();

        // 2. Near and Far points in World Space
        Vec3 nearPoint = invVP.Transform(Vec3(x, y, 0.01)); // Near plane
        Vec3 farPoint = invVP.Transform(Vec3(x, y, 1.0));  // Far plane

        return Ray(nearPoint, farPoint - nearPoint);
    }

    /** @brief 3D Dünya koordinatını 2D Ekran koordinatına (Piksel) çevirir. */
    Vec3 WorldToScreen(const Vec3& worldPos, float width, float height) const {
        Mat4 mvp = GetProjectionMatrix() * GetViewMatrix();
        Vec3 ndc = mvp.Transform(worldPos);
        return {
            (ndc.x + 1.0f) * width / 2.0f,
            (1.0f - ndc.y) * height / 2.0f,
            ndc.z
        };
    }
};

/** @brief Dereceyi radyana çevirir. */
inline double ToRadians(double deg) { return deg * PI / 180.0; }

} // namespace geom
} // namespace vkt
