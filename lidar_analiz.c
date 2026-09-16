#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif

#define MAX_POINTS 1000
#define MAX_LINES 100
#define MIN_LINE_POINTS 8
#define RANSAC_ITERATIONS 2000
#define RANSAC_THRESHOLD 0.05
#define ANGLE_THRESHOLD 60.0
#define PI 3.14159265358979323846

typedef struct
{
    double x;
    double y;
} Point;

typedef struct
{
    double a, b, c;
    int point_count;
    int point_indices[MAX_POINTS];
} Line;

typedef struct
{
    double angle_min;
    double angle_max;
    double angle_increment;
    double range_min;
    double range_max;
    double ranges[MAX_POINTS];
    int range_count;
} LidarData;

const char *LIDAR_URLS[] = {
    "http://abilgisayar.kocaeli.edu.tr/lidar1.toml",
    "http://abilgisayar.kocaeli.edu.tr/lidar2.toml",
    "http://abilgisayar.kocaeli.edu.tr/lidar3.toml",
    "http://abilgisayar.kocaeli.edu.tr/lidar4.toml",
    "http://abilgisayar.kocaeli.edu.tr/lidar5.toml"};
const int URL_COUNT = 5;

void initNetwork()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed!\n");
        exit(1);
    }
#endif
}

void cleanupNetwork()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

int downloadFile(const char *url, const char *filename)
{
    printf("\n>>> INDIRME BASLIYOR <<<\n");
    printf("URL: %s\n", url);

    if (strncmp(url, "https://", 8) == 0)
    {
        printf("HTTPS tespit edildi, curl kullaniliyor...\n");

#ifdef _WIN32
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "curl -s -S -L -o \"%s\" \"%s\" 2>nul", filename, url);

        printf("Curl calistiriliyor...\n");
        int result = system(cmd);

        if (result == 0)
        {

            FILE *check = fopen(filename, "rb");
            if (check)
            {
                fseek(check, 0, SEEK_END);
                long size = ftell(check);
                fclose(check);

                printf("Indirilen: %ld byte\n", size);

                if (size > 50)
                {
                    printf("\n>>> BASARILI! <<<\n\n");
                    return 1;
                }
            }
        }

        printf("Curl basarisiz oldu, HTTP denemesi yapiliyor...\n");
#endif
    }

    char modified_url[1024];
    if (strncmp(url, "https://", 8) == 0)
    {
        snprintf(modified_url, sizeof(modified_url), "http://%s", url + 8);
        printf("HTTP deneniyor: %s\n", modified_url);
        url = modified_url;
    }

    char host[256], path[512];
    int port = 80;

    const char *start = url;
    if (strncmp(url, "http://", 7) == 0)
    {
        start += 7;
    }

    const char *slash = strchr(start, '/');
    if (slash)
    {
        size_t len = slash - start;
        strncpy(host, start, len);
        host[len] = '\0';
        strcpy(path, slash);
    }
    else
    {
        strcpy(host, start);
        strcpy(path, "/");
    }

    char *colon = strchr(host, ':');
    if (colon)
    {
        *colon = '\0';
        port = atoi(colon + 1);
    }

    printf("[1] Host: %s\n", host);
    printf("[2] Port: %d\n", port);
    printf("[3] Path: %s\n", path);

#ifdef _WIN32
    SOCKET sock;
    struct sockaddr_in server;
    struct hostent *he;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        printf("XXX Socket hatasi: %d\n", WSAGetLastError());
        return 0;
    }

    printf("[4] Socket olusturuldu\n");

    int timeout = 15000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (strcmp(host, "localhost") == 0)
    {
        server.sin_addr.s_addr = inet_addr("127.0.0.1");
        printf("[5] Localhost kullaniliyor\n");
    }
    else
    {
        printf("[5] DNS cozumleniyor: %s\n", host);
        he = gethostbyname(host);
        if (!he)
        {
            printf("XXX DNS hatasi: %d\n", WSAGetLastError());
            closesocket(sock);
            return 0;
        }
        memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
        printf("[5] IP: %s\n", inet_ntoa(server.sin_addr));
    }

    printf("[6] Baglaniyor...\n");
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("XXX Baglanti hatasi: %d\n", WSAGetLastError());
        closesocket(sock);
        return 0;
    }

    printf("[7] BAGLANDI!\n");

    char request[2048];
    sprintf(request,
            "GET %s HTTP/1.0\r\n"
            "Host: %s\r\n"
            "User-Agent: Mozilla/5.0\r\n"
            "Accept: */*\r\n"
            "\r\n",
            path, host);

    printf("[8] HTTP istegi gonderiliyor...\n");
    if (send(sock, request, strlen(request), 0) < 0)
    {
        printf("XXX Send hatasi: %d\n", WSAGetLastError());
        closesocket(sock);
        return 0;
    }

    printf("[9] Yanit aliniyor...\n");

    FILE *f = fopen(filename, "wb");
    if (!f)
    {
        printf("XXX Dosya acilamadi!\n");
        closesocket(sock);
        return 0;
    }

    char buf[8192];
    int total = 0;
    int header_parsed = 0;
    char header_buf[4096] = {0};
    int header_len = 0;

    while (1)
    {
        int n = recv(sock, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
        {
            if (n < 0)
            {
                printf("\nRecv hatasi: %d\n", WSAGetLastError());
            }
            break;
        }

        buf[n] = '\0';

        if (!header_parsed)
        {

            if (header_len + n < sizeof(header_buf))
            {
                memcpy(header_buf + header_len, buf, n);
                header_len += n;
                header_buf[header_len] = '\0';
            }

            char *body_start = strstr(header_buf, "\r\n\r\n");
            if (body_start)
            {

                int status = 0;
                if (sscanf(header_buf, "HTTP/%*d.%*d %d", &status) == 1)
                {
                    printf("[10] HTTP Status: %d\n", status);
                    if (status == 301 || status == 302 || status == 307 || status == 308)
                    {
                        printf("UYARI: Redirect yaniti! GitHub HTTPS zorunlu kilmis olabilir.\n");
                    }
                }

                body_start += 4;
                int header_total = (body_start - header_buf);
                int body_in_header = header_len - header_total;

                if (body_in_header > 0)
                {
                    fwrite(body_start, 1, body_in_header, f);
                    total += body_in_header;
                    printf("[11] Body basliyor (%d byte)\n", body_in_header);
                }

                header_parsed = 1;
            }
        }
        else
        {
            fwrite(buf, 1, n, f);
            total += n;
            printf("\r    Indirilen: %d byte", total);
            fflush(stdout);
        }
    }

    printf("\n");
    fclose(f);
    closesocket(sock);

    printf("[12] Kapandi. Toplam: %d byte\n", total);

    if (total > 50)
    {
        printf("\n>>> BASARILI! <<<\n\n");
        return 1;
    }
    else
    {
        printf("\n>>> BASARISIZ (cok kisa dosya veya redirect) <<<\n\n");
        printf("NOT: GitHub HTTPS zorluyor olabilir. Curl kullanmayi deneyin:\n");
        printf("     curl -o %s %s\n", filename, url);
        remove(filename);
        return 0;
    }

#else
    // Linux/Mac için curl
    char cmd[1024];
    sprintf(cmd, "curl -s -L -o '%s' '%s'", filename, url);
    return (system(cmd) == 0);
#endif
}

char *trim(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++;
    if (*str == 0)
        return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return str;
}

int readTOMLFile(const char *filename, LidarData *data)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("HATA: Dosya acilamadi: %s\n", filename);
        return 0;
    }

    char line[8192];
    int in_ranges = 0;
    data->range_count = 0;

    printf("\n=== TOML DOSYASI OKUNUYOR: %s ===\n", filename);

    while (fgets(line, sizeof(line), file))
    {
        char *trimmed = trim(line);

        if (strlen(trimmed) == 0 || trimmed[0] == '#')
            continue;

        if (strcmp(trimmed, "[header]") == 0 || strcmp(trimmed, "[scan]") == 0)
        {
            continue;
        }

        if (strstr(trimmed, "angle_min"))
        {
            char *eq = strchr(trimmed, '=');
            if (eq)
            {
                data->angle_min = atof(eq + 1);
                printf("angle_min: %f\n", data->angle_min);
            }
        }
        else if (strstr(trimmed, "angle_max"))
        {
            char *eq = strchr(trimmed, '=');
            if (eq)
            {
                data->angle_max = atof(eq + 1);
                printf("angle_max: %f\n", data->angle_max);
            }
        }
        else if (strstr(trimmed, "angle_increment"))
        {
            char *eq = strchr(trimmed, '=');
            if (eq)
            {
                data->angle_increment = atof(eq + 1);
                printf("angle_increment: %f\n", data->angle_increment);
            }
        }
        else if (strstr(trimmed, "range_min"))
        {
            char *eq = strchr(trimmed, '=');
            if (eq)
            {
                data->range_min = atof(eq + 1);
                printf("range_min: %f\n", data->range_min);
            }
        }
        else if (strstr(trimmed, "range_max"))
        {
            char *eq = strchr(trimmed, '=');
            if (eq)
            {
                data->range_max = atof(eq + 1);
                printf("range_max: %f\n", data->range_max);
            }
        }
        else if (strstr(trimmed, "ranges"))
        {
            in_ranges = 1;
            char *bracket = strchr(trimmed, '[');
            if (bracket)
            {
                trimmed = bracket + 1;
            }
            else
            {
                continue;
            }
        }

        if (in_ranges)
        {
            char *ptr = trimmed;
            while (*ptr && data->range_count < MAX_POINTS)
            {
                while (*ptr && (isspace(*ptr) || *ptr == ','))
                    ptr++;

                if (*ptr == ']')
                {
                    in_ranges = 0;
                    break;
                }

                if (*ptr == '-' || *ptr == '.' || isdigit(*ptr))
                {
                    double value = atof(ptr);
                    data->ranges[data->range_count++] = value;

                    while (*ptr && *ptr != ',' && *ptr != ']' && !isspace(*ptr))
                        ptr++;
                }
                else
                {
                    ptr++;
                }
            }
        }
    }

    fclose(file);

    printf("\n=== OKUMA TAMAMLANDI ===\n");
    printf("Toplam mesafe degeri: %d\n", data->range_count);
    printf("Aci araligi: %.4f rad ile %.4f rad arasi\n", data->angle_min, data->angle_max);
    printf("Aci artisi: %.6f rad\n", data->angle_increment);

    return 1;
}

void filterAndConvertToCartesian(LidarData *data, Point *points, int *point_count)
{
    *point_count = 0;
    int filtered_nan = 0, filtered_negative = 0, filtered_min = 0, filtered_max = 0;

    printf("\n=== FILTRELEME VE KARTEZYEN DONUSUM ===\n");
    printf("Range_min: %.2f, Range_max: %.2f\n", data->range_min, data->range_max);

    for (int i = 0; i < data->range_count; i++)
    {
        double range = data->ranges[i];

        if (i < 10)
        {
            printf("Range[%d]: %.2f -> ", i, range);
        }

        if (range == -1.0 || range == 999.0 || range == -999.0)
        {
            if (i < 10)
                printf("FILTRELENDI (NaN)\n");
            filtered_nan++;
            continue;
        }

        if (range < 0)
        {
            if (i < 10)
                printf("FILTRELENDI (Negatif)\n");
            filtered_negative++;
            continue;
        }

        if (range < data->range_min)
        {
            if (i < 10)
                printf("FILTRELENDI (Min alti)\n");
            filtered_min++;
            continue;
        }

        if (range > data->range_max)
        {
            if (i < 10)
                printf("FILTRELENDI (Max ustu)\n");
            filtered_max++;
            continue;
        }

        double angle = data->angle_min + i * data->angle_increment;
        points[*point_count].x = range * cos(angle);
        points[*point_count].y = range * sin(angle);

        if (i < 10)
            printf("Kabul: (%.2f, %.2f)\n", points[*point_count].x, points[*point_count].y);

        (*point_count)++;
    }

    printf("Gecerli nokta sayisi: %d\n", *point_count);
    printf("Filtrelenen: Gecersiz=%d, Negatif=%d, Min=%d, Max=%d\n",
           filtered_nan, filtered_negative, filtered_min, filtered_max);
}

double pointDistance(Point p1, Point p2)
{
    return sqrt((p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y));
}

double pointToLineDistance(Point p, Line line)
{
    return fabs(line.a * p.x + line.b * p.y + line.c) /
           sqrt(line.a * line.a + line.b * line.b);
}

void computeLine(Point p1, Point p2, Line *line)
{
    line->a = p2.y - p1.y;
    line->b = p1.x - p2.x;
    line->c = (p2.x - p1.x) * p1.y - (p2.y - p1.y) * p1.x;

    double norm = sqrt(line->a * line->a + line->b * line->b);
    if (norm > 0.0001)
    {
        line->a /= norm;
        line->b /= norm;
        line->c /= norm;
    }
}

int detectLines(Point *points, int point_count, Line *lines, int *used)
{
    int line_count = 0;

    printf("\n=== RANSAC DOGRU TESPITI ===\n");

    for (int i = 0; i < point_count; i++)
    {
        used[i] = 0;
    }

    while (line_count < MAX_LINES)
    {
        int available_count = 0;
        for (int i = 0; i < point_count; i++)
        {
            if (!used[i])
                available_count++;
        }

        if (available_count < MIN_LINE_POINTS)
            break;

        Line best_line;
        int best_inliers[MAX_POINTS];
        int best_inlier_count = 0;

        for (int iter = 0; iter < RANSAC_ITERATIONS; iter++)
        {
            int idx1, idx2;
            do
            {
                idx1 = rand() % point_count;
            } while (used[idx1]);

            do
            {
                idx2 = rand() % point_count;
            } while (used[idx2] || idx1 == idx2);

            if (pointDistance(points[idx1], points[idx2]) < 0.1)
                continue;

            Line candidate_line;
            computeLine(points[idx1], points[idx2], &candidate_line);

            int inliers[MAX_POINTS];
            int inlier_count = 0;

            for (int i = 0; i < point_count; i++)
            {
                if (used[i])
                    continue;

                double dist = pointToLineDistance(points[i], candidate_line);
                if (dist < RANSAC_THRESHOLD)
                {
                    inliers[inlier_count++] = i;
                }
            }

            if (inlier_count > best_inlier_count && inlier_count >= MIN_LINE_POINTS)
            {
                best_inlier_count = inlier_count;
                best_line = candidate_line;
                memcpy(best_inliers, inliers, inlier_count * sizeof(int));
            }
        }

        if (best_inlier_count >= MIN_LINE_POINTS)
        {
            lines[line_count] = best_line;
            lines[line_count].point_count = best_inlier_count;

            for (int i = 0; i < best_inlier_count; i++)
            {
                lines[line_count].point_indices[i] = best_inliers[i];
                used[best_inliers[i]] = 1;
            }

            printf("Dogru %d: %d nokta\n", line_count + 1, best_inlier_count);

            line_count++;
        }
        else
        {
            break;
        }
    }

    printf("Toplam dogru: %d\n", line_count);

    printf("\n=== KISA DOGRULAR FILTRELENIYOR ===\n");

    int original_count = line_count;

    for (int i = 0; i < line_count; i++)
    {

        double min_x = 999999, max_x = -999999;
        double min_y = 999999, max_y = -999999;

        for (int j = 0; j < lines[i].point_count; j++)
        {
            int idx = lines[i].point_indices[j];

            if (points[idx].x < min_x)
                min_x = points[idx].x;
            if (points[idx].x > max_x)
                max_x = points[idx].x;
            if (points[idx].y < min_y)
                min_y = points[idx].y;
            if (points[idx].y > max_y)
                max_y = points[idx].y;
        }

        double length = sqrt((max_x - min_x) * (max_x - min_x) +
                             (max_y - min_y) * (max_y - min_y));

        if (length < 0.30)
        {
            printf("Dogru %d SILINDI: %.2fm (cok kisa), %d nokta\n",
                   i + 1, length, lines[i].point_count);

            for (int j = 0; j < lines[i].point_count; j++)
            {
                used[lines[i].point_indices[j]] = 0;
            }

            for (int k = i; k < line_count - 1; k++)
            {
                lines[k] = lines[k + 1];
            }

            line_count--;
            i--;
        }
        else
        {
            printf("Dogru %d: %.2fm, %d nokta - KABUL\n",
                   i + 1, length, lines[i].point_count);
        }
    }

    printf("\nFiltreleme sonucu:\n");
    printf("  Baslangic: %d dogru\n", original_count);
    printf("  Silinen: %d dogru\n", original_count - line_count);
    printf("  Kalan: %d dogru\n", line_count);

    return line_count;
}

int lineIntersection(Line l1, Line l2, Point *intersection)
{
    double det = l1.a * l2.b - l2.a * l1.b;

    if (fabs(det) < 0.0001)
        return 0;

    intersection->x = (l1.b * l2.c - l2.b * l1.c) / det;
    intersection->y = (l2.a * l1.c - l1.a * l2.c) / det;

    return 1;
}

int segmentIntersection(Line l1, Line l2,
                        Point *points, Point *intersection)
{

    double det = l1.a * l2.b - l2.a * l1.b;
    if (fabs(det) < 0.0001)
        return 0;

    intersection->x = (l1.b * l2.c - l2.b * l1.c) / det;
    intersection->y = (l2.a * l1.c - l1.a * l2.c) / det;

    double l1_min_x = 999999, l1_max_x = -999999;
    double l1_min_y = 999999, l1_max_y = -999999;

    for (int i = 0; i < l1.point_count; i++)
    {
        int idx = l1.point_indices[i];
        if (points[idx].x < l1_min_x)
            l1_min_x = points[idx].x;
        if (points[idx].x > l1_max_x)
            l1_max_x = points[idx].x;
        if (points[idx].y < l1_min_y)
            l1_min_y = points[idx].y;
        if (points[idx].y > l1_max_y)
            l1_max_y = points[idx].y;
    }

    double l2_min_x = 999999, l2_max_x = -999999;
    double l2_min_y = 999999, l2_max_y = -999999;

    for (int i = 0; i < l2.point_count; i++)
    {
        int idx = l2.point_indices[i];
        if (points[idx].x < l2_min_x)
            l2_min_x = points[idx].x;
        if (points[idx].x > l2_max_x)
            l2_max_x = points[idx].x;
        if (points[idx].y < l2_min_y)
            l2_min_y = points[idx].y;
        if (points[idx].y > l2_max_y)
            l2_max_y = points[idx].y;
    }

    double margin = 0.2;

    int in_l1 = (intersection->x >= l1_min_x - margin &&
                 intersection->x <= l1_max_x + margin &&
                 intersection->y >= l1_min_y - margin &&
                 intersection->y <= l1_max_y + margin);

    int in_l2 = (intersection->x >= l2_min_x - margin &&
                 intersection->x <= l2_max_x + margin &&
                 intersection->y >= l2_min_y - margin &&
                 intersection->y <= l2_max_y + margin);

    return (in_l1 && in_l2);
}

double angleBetweenLines(Line l1, Line l2)
{
    double dot = l1.a * l2.a + l1.b * l2.b;
    double angle_rad = acos(fabs(dot));
    double angle_deg = angle_rad * 180.0 / PI;
    return angle_deg;
}

void findIntersectionsAndAngles(Line *lines, int line_count,
                                Point *intersections, int *intersection_count,
                                double *angles, int *line_pairs,
                                Point *points)
{
    *intersection_count = 0;

    printf("\n=== KESISIM ANALIZI ===\n");

    for (int i = 0; i < line_count; i++)
    {
        for (int j = i + 1; j < line_count; j++)
        {
            Point intersection;

            if (segmentIntersection(lines[i], lines[j], points, &intersection))
            {
                double angle = angleBetweenLines(lines[i], lines[j]);

                if (angle >= ANGLE_THRESHOLD)
                {
                    double distance = sqrt(intersection.x * intersection.x +
                                           intersection.y * intersection.y);
                    printf("Dogru %d-%d: Aci=%.1f°, Kesisim=(%.2f,%.2f), Mesafe=%.2f\n",
                           i + 1, j + 1, angle, intersection.x, intersection.y, distance);

                    intersections[*intersection_count] = intersection;
                    angles[*intersection_count] = angle;
                    line_pairs[*intersection_count * 2] = i;
                    line_pairs[*intersection_count * 2 + 1] = j;
                    (*intersection_count)++;
                }
            }
        }
    }

    printf("Toplam %d derece ustu kesisim: %d\n", (int)ANGLE_THRESHOLD, *intersection_count);
}

void visualize(Point *points, int point_count, Line *lines, int line_count,
               Point *intersections, int intersection_count,
               double *angles, int *line_pairs)
{
    printf("\n=== GORSELLESTIRME ===\n");

    remove("lidar_data.txt");
    remove("lidar_plot.png");

    FILE *fp = fopen("lidar_data.txt", "w");
    if (!fp)
    {
        printf("HATA: lidar_data.txt dosyasi olusturulamadi!\n");
        return;
    }

    fprintf(fp, "POINTS\n");
    for (int i = 0; i < point_count; i++)
    {
        fprintf(fp, "%f %f\n", points[i].x, points[i].y);
    }

    fprintf(fp, "LINES\n");
    for (int i = 0; i < line_count; i++)
    {
        fprintf(fp, "%f %f %f\n", lines[i].a, lines[i].b, lines[i].c);
    }

    fprintf(fp, "INTERSECTIONS\n");
    for (int i = 0; i < intersection_count; i++)
    {
        double dist = sqrt(intersections[i].x * intersections[i].x +
                           intersections[i].y * intersections[i].y);
        fprintf(fp, "%f %f %f %f\n",
                intersections[i].x,
                intersections[i].y,
                angles[i],
                dist);
    }

    fprintf(fp, "LINE_PAIRS\n");
    for (int i = 0; i < intersection_count; i++)
    {
        fprintf(fp, "%d %d\n", line_pairs[i * 2], line_pairs[i * 2 + 1]);
    }

    fclose(fp);
    printf("Veri dosyaya yazildi: lidar_data.txt\n");

    printf("Python scripti calistiriliyor...\n");
    int result = 0;

#ifdef _WIN32
    result = system("python plot_lidar.py");
    if (result != 0)
    {
        result = system("py plot_lidar.py");
    }
#else
    result = system("python3 plot_lidar.py");
    if (result != 0)
    {
        result = system("python plot_lidar.py");
    }
#endif

    if (result == 0)
    {
        printf("\n*** BASARILI! ***\n");
        printf("Grafik kaydedildi: lidar_plot.png\n");

#ifdef _WIN32
        system("start lidar_plot.png");
#endif
    }
    else
    {
        printf("\n*** UYARI! ***\n");
        printf("Python scripti calistirilamadi (result code: %d).\n", result);
    }
}

int main()
{
    srand(42);
    initNetwork();

    char filename[256];
    int choice;

    printf("======================================\n");
    printf("  LIDAR ANALIZ PROGRAMI\n");
    printf("======================================\n");
    printf("Veri Kaynagi Secimi:\n");
    printf("1. Klasorden 'scan_data_nan.toml' dosyasini kullan (Varsayilan)\n");
    printf("2. Proje linklerinden birini indir ve kullan\n");
    printf("3. Harici bir dosya/link adi gir\n");
    printf("Seciminiz (1/2/3): ");

    if (scanf("%d", &choice) != 1)
    {
        printf("HATA: Gecersiz giris.\n");
        cleanupNetwork();
        return 1;
    }

    while (getchar() != '\n')
        ;

    if (choice == 1)
    {
        strcpy(filename, "scan_data_nan.toml");
        printf("-> 'scan_data_nan.toml' dosyasi kullanilacak.\n");
    }
    else if (choice == 2)
    {
        printf("\nIndirilecek Link Secimi:\n");
        for (int i = 0; i < URL_COUNT; i++)
        {
            printf("%d. %s\n", i + 1, LIDAR_URLS[i]);
        }
        printf("Link numarasi (1-%d): ", URL_COUNT);

        int link_choice;
        if (scanf("%d", &link_choice) != 1 || link_choice < 1 || link_choice > URL_COUNT)
        {
            printf("HATA: Gecersiz link numarasi.\n");
            cleanupNetwork();
            return 1;
        }

        while (getchar() != '\n')
            ;

        const char *url = LIDAR_URLS[link_choice - 1];
        strcpy(filename, "downloaded_scan.toml");

        if (!downloadFile(url, filename))
        {
            printf("Internet baglantisi yok veya indirme basarisiz.\n");
            printf("Yerel 'scan_data_nan.toml' dosyasi kullanilacak.\n");
            strcpy(filename, "scan_data_nan.toml");
        }
    }
    else if (choice == 3)
    {
        char input_link[256];
        printf("Lutfen dosya yolunu veya linki (http://...) giriniz: ");
        if (scanf("%255s", input_link) != 1)
        {
            printf("HATA: Gecersiz giris.\n");
            cleanupNetwork();
            return 1;
        }

        if (strncmp(input_link, "http://", 7) == 0 || strncmp(input_link, "https://", 8) == 0)
        {
            char download_name[256];
            strcpy(download_name, "custom_download.toml");

            printf("Link tespit edildi, indiriliyor...\n");

            if (!downloadFile(input_link, download_name))
            {
                printf("Indirme basarisiz!\n");
                printf("Yerel 'scan_data_nan.toml' dosyasi kullanilacak.\n");
                strcpy(filename, "scan_data_nan.toml");
            }
            else
            {
                strcpy(filename, download_name);
                printf("Indirilen dosya kullanilacak: %s\n", filename);
            }
        }
        else
        {

            strcpy(filename, input_link);
            printf("-> Lokal dosya kullanilacak: '%s'\n", filename);
        }
    }

    printf("\n=== SECILEN DOSYA: %s ===\n", filename);

    LidarData lidar_data;
    Point points[MAX_POINTS];
    int point_count = 0;
    Line lines[MAX_LINES];
    int line_count = 0;
    int used[MAX_POINTS];
    Point intersections[MAX_LINES * MAX_LINES];
    int intersection_count = 0;
    double angles[MAX_LINES * MAX_LINES];
    int line_pairs[MAX_LINES * MAX_LINES * 2];

    if (!readTOMLFile(filename, &lidar_data))
    {
        printf("Dosya okuma basarisiz! Program sonlandiriliyor.\n");
        cleanupNetwork();
        return 1;
    }

    filterAndConvertToCartesian(&lidar_data, points, &point_count);

    if (point_count < MIN_LINE_POINTS)
    {
        printf("Yeterli nokta yok! (%d < %d)\n", point_count, MIN_LINE_POINTS);
        cleanupNetwork();
        return 1;
    }

    line_count = detectLines(points, point_count, lines, used);

    if (line_count < 2)
    {
        printf("En az 2 dogru bulunamadi! (%d dogru)\n", line_count);
        cleanupNetwork();
        return 1;
    }

    findIntersectionsAndAngles(lines, line_count, intersections, &intersection_count,
                               angles, line_pairs, points);

    visualize(points, point_count, lines, line_count,
              intersections, intersection_count, angles, line_pairs);

    printf("\n======================================\n");
    printf("  ANALIZ TAMAMLANDI\n");
    printf("======================================\n");

    cleanupNetwork();

#ifdef _WIN32
    printf("\nProgram 2 saniye sonra kapanacak ve exe silinecek...\n");
    Sleep(2000);

    FILE *ps = fopen("_cleanup.ps1", "w");
    if (ps)
    {
        fprintf(ps, "Start-Sleep -Milliseconds 500\n");
        fprintf(ps, "while (Test-Path 'proje1.exe') {\n");
        fprintf(ps, "    try {\n");
        fprintf(ps, "        Remove-Item 'proje1.exe' -Force -ErrorAction Stop\n");
        fprintf(ps, "        Write-Host 'Exe silindi!'\n");
        fprintf(ps, "        break\n");
        fprintf(ps, "    } catch {\n");
        fprintf(ps, "        Start-Sleep -Milliseconds 200\n");
        fprintf(ps, "    }\n");
        fprintf(ps, "}\n");
        fprintf(ps, "Remove-Item '_cleanup.ps1' -Force -ErrorAction SilentlyContinue\n");
        fclose(ps);

        system("start /b powershell -WindowStyle Hidden -ExecutionPolicy Bypass -File _cleanup.ps1");
    }
#endif

    return 0;
}
