import numpy as np
import matplotlib.pyplot as plt
import os

print("Python scripti calisiyor...")

try:
    with open('lidar_data.txt', 'r', encoding='utf-8') as f:
        lines = f.readlines()
except FileNotFoundError:
    print("HATA: lidar_data.txt bulunamadi!")
    exit(1)

points_x, points_y = [], []
line_params = []
inter_x, inter_y, angles, distances = [], [], [], []
line_pairs = []

section = None
for line in lines:
    line = line.strip()
    
    if line == 'POINTS':
        section = 'points'
        continue
    elif line == 'LINES':
        section = 'lines'
        continue
    elif line == 'INTERSECTIONS':
        section = 'intersections'
        continue
    elif line == 'LINE_PAIRS':
        section = 'line_pairs'
        continue
    
    if not line:
        continue
    
    try:
        if section == 'points':
            x, y = map(float, line.split())
            points_x.append(x)
            points_y.append(y)
        elif section == 'lines':
            a, b, c = map(float, line.split())
            line_params.append((a, b, c))
        elif section == 'intersections':
            x, y, angle, dist = map(float, line.split())
            inter_x.append(x)
            inter_y.append(y)
            angles.append(angle)
            distances.append(dist)
        elif section == 'line_pairs':
            i, j = map(int, line.split())
            line_pairs.append((i, j))
    except ValueError:
        continue

print(f"Okunan veri: {len(points_x)} nokta, {len(line_params)} dogru, {len(inter_x)} kesisim")

# Grafik oluştur - Daha geniş ve bilgi kutuları için alan bırak
fig = plt.figure(figsize=(20, 12))
ax = plt.subplot(111)

# NOKTALARIN SINIRLARINI BUL
if points_x and points_y:
    x_min, x_max = min(points_x), max(points_x)
    y_min, y_max = min(points_y), max(points_y)
    margin = 0.5
else:
    x_min, x_max, y_min, y_max = -3, 3, -3, 3
    margin = 0.5

# 1. LIDAR noktalarını çiz
if points_x and points_y:
    ax.scatter(points_x, points_y, c='lightblue', s=35, alpha=0.7, 
               label='LIDAR Noktalari', zorder=2, edgecolors='steelblue', linewidths=0.5)

# 2. Robot konumunu çiz (0,0)
ax.scatter(0, 0, c='red', s=350, marker='o', 
           edgecolors='darkred', linewidths=3, 
           label='Robot', zorder=10)

# 3. DOĞRULARI ÇİZ - FARKLI RENKLER
colors = [
    '#006400',  # Dark Green
    '#FF4500',  # Orange Red
    '#0000FF',  # Blue
    '#FFD700',  # Gold
    '#8B008B',  # Dark Magenta
    '#00CED1',  # Dark Turquoise
    '#FF1493',  # Deep Pink
    '#2F4F4F',  # Dark Slate Gray
    '#DC143C',  # Crimson
    '#228B22',  # Forest Green
    '#4169E1',  # Royal Blue
    '#FF8C00'   # Dark Orange
]

# Sadece kesişimde kullanılan doğrular
used_lines = set()
for i, j in line_pairs:
    used_lines.add(i)
    used_lines.add(j)

for line_idx in sorted(used_lines):
    if line_idx >= len(line_params):
        continue
    
    a, b, c = line_params[line_idx]
    color = colors[line_idx % len(colors)]
    
    # Bu doğruya yakın noktaları bul
    line_points = []
    for px, py in zip(points_x, points_y):
        dist_to_line = abs(a * px + b * py + c) / np.sqrt(a*a + b*b)
        if dist_to_line < 0.05:
            line_points.append((px, py))
    
    if len(line_points) < 2:
        continue
    
    line_points = np.array(line_points)
    
    # Doğru üzerindeki noktaların min/max'ini bul
    if abs(b) > 1e-10:
        sorted_points = line_points[np.argsort(line_points[:, 0])]
        x_start, x_end = sorted_points[0, 0], sorted_points[-1, 0]
        y_start = -(a * x_start + c) / b
        y_end = -(a * x_end + c) / b
    else:
        sorted_points = line_points[np.argsort(line_points[:, 1])]
        x_start = x_end = -c / a
        y_start, y_end = sorted_points[0, 1], sorted_points[-1, 1]
    
    # DOĞRUNUN FİZİKSEL UZUNLUĞU (metre)
    line_length = np.sqrt((x_end - x_start)**2 + (y_end - y_start)**2)
    
    # Doğruyu çiz
    ax.plot([x_start, x_end], [y_start, y_end], 
            color=color, linewidth=4, alpha=0.85, 
            label=f'd{line_idx+1}',
            zorder=3)

# 4. KESİŞİM NOKTALARINI ÇİZ
if inter_x and inter_y:
    # Mesafeye göre sırala
    intersection_data = list(zip(inter_x, inter_y, angles, distances, range(len(inter_x))))
    intersection_data.sort(key=lambda x: x[3])  # mesafeye göre sırala
    
    # En yakın kesişim
    closest = intersection_data[0]
    closest_x, closest_y, closest_angle, closest_dist = closest[0], closest[1], closest[2], closest[3]
    
    # TÜM kesişim noktalarını çiz (sarı kareler) - TÜM NUMARALARI GÖSTER
    for idx, (x, y, angle, dist, orig_idx) in enumerate(intersection_data):
        ax.scatter(x, y, c='yellow', s=280, marker='s', 
                   edgecolors='black', linewidths=2.5, zorder=8,
                   label='Kesisim (60+)' if idx == 0 else "")
        
        # NUMARA YAZ - Daha fazla kesişim için de numara göster
        ax.text(x, y, f'{idx+1}', 
               fontsize=10, fontweight='bold', 
               ha='center', va='center',
               color='black', zorder=9)
    
    # SADECE EN YAKINA mesafe çizgisi
    ax.plot([0, closest_x], [0, closest_y], 'r--', linewidth=3.5, alpha=0.85, 
            zorder=4, label=f'En yakin: {closest_dist:.2f}m')
    
    # Mesafe etiketini çizginin ortasına yaz - SADECE EN YAKINA
    mid_x, mid_y = closest_x/2, closest_y/2
    ax.text(mid_x, mid_y, f'En Yakın (#1)\n{closest_dist:.2f}m', 
            bbox=dict(boxstyle='round,pad=0.6', facecolor='orange', 
                     alpha=0.95, edgecolor='black', linewidth=2.5),
            fontsize=12, fontweight='bold', ha='center', color='black',
            zorder=9)

# SOL ÜSTTE - DOĞRU RENK LEJANTİ (Axes koordinatlarında) - DÜZELTİLDİ
if used_lines:
    legend_text = "DOĞRU RENKLERİ\n" + "="*20 + "\n"
    
    # Sol üst köşeye yerleştir
    text_box = ax.text(0.02, 0.98, legend_text,
            transform=ax.transAxes,
            fontsize=10,
            verticalalignment='top',
            horizontalalignment='left',
            bbox=dict(boxstyle='round,pad=0.7', facecolor='white', 
                     alpha=0.95, edgecolor='black', linewidth=2),
            family='monospace',
            zorder=15)
    
    # Renkli çizgiler ve etiketler ekle (manuel olarak)
    y_start = 0.955
    for line_idx in sorted(used_lines):
        if line_idx >= len(line_params):
            continue
        color = colors[line_idx % len(colors)]
        
        # Renkli çizgi
        ax.plot([0.025, 0.065], [y_start, y_start], 
                transform=ax.transAxes,
                color=color, linewidth=6, solid_capstyle='round', zorder=16)
        
        # Doğru ismi
        ax.text(0.072, y_start, f'd{line_idx+1}',
                transform=ax.transAxes,
                fontsize=10,
                verticalalignment='center',
                horizontalalignment='left',
                family='monospace',
                fontweight='bold',
                zorder=16)
        
        y_start -= 0.028  # Bir sonraki satıra

# SAĞ ÜSTTE - GENEL İSTATİSTİK (Figure koordinatlarında)
info_text = "GENEL ANALİZ\n" + "="*22 + "\n\n"
info_text += f"LIDAR Nokta   : {len(points_x)}\n"
info_text += f"Tespit Doğru  : {len(used_lines)}\n"
info_text += f"Kesişim (60°+): {len(inter_x)}\n"

fig.text(0.77, 0.93, info_text,
        fontsize=10.5,
        verticalalignment='top',
        horizontalalignment='left',
        bbox=dict(boxstyle='round,pad=0.8', facecolor='lightgray', 
                 alpha=0.95, edgecolor='black', linewidth=2.5),
        family='monospace',
        zorder=20)

# SAĞ ÜSTTE - DOĞRU BİLGİLERİ TABLOSU (Figure koordinatlarında)
if used_lines:
    table_text = "DOĞRU BİLGİLERİ\n" + "="*40 + "\n"
    table_text += "No  Nokta  Uzunluk(m)  Renk\n"
    table_text += "-"*40 + "\n"
    
    color_names_tr = {
        '#006400': 'Koyu Yeşil',
        '#FF4500': 'Turuncu Kırmızı',
        '#0000FF': 'Mavi',
        '#FFD700': 'Altın Sarısı',
        '#8B008B': 'Mor',
        '#00CED1': 'Turkuaz',
        '#FF1493': 'Pembe',
        '#2F4F4F': 'Koyu Gri',
        '#DC143C': 'Kırmızı',
        '#228B22': 'Orman Yeşili',
        '#4169E1': 'Kraliyet Mavisi',
        '#FF8C00': 'Koyu Turuncu'
    }
    
    for line_idx in sorted(used_lines):
        if line_idx >= len(line_params):
            continue
        
        a, b, c = line_params[line_idx]
        color_hex = colors[line_idx % len(colors)]
        color_name = color_names_tr.get(color_hex, 'Renkli')
        
        # Bu doğruya ait nokta sayısı ve uzunluğu
        line_points = []
        for px, py in zip(points_x, points_y):
            dist_to_line = abs(a * px + b * py + c) / np.sqrt(a*a + b*b)
            if dist_to_line < 0.05:
                line_points.append((px, py))
        
        if len(line_points) >= 2:
            line_points = np.array(line_points)
            if abs(b) > 1e-10:
                sorted_points = line_points[np.argsort(line_points[:, 0])]
                x_start, x_end = sorted_points[0, 0], sorted_points[-1, 0]
                y_start = -(a * x_start + c) / b
                y_end = -(a * x_end + c) / b
            else:
                sorted_points = line_points[np.argsort(line_points[:, 1])]
                x_start = x_end = -c / a
                y_start, y_end = sorted_points[0, 1], sorted_points[-1, 1]
            
            line_length = np.sqrt((x_end - x_start)**2 + (y_end - y_start)**2)
            table_text += f"d{line_idx+1:2d}  {len(line_points):4d}    {line_length:6.2f}     {color_name}\n"
    
    table_text += "\npnt = nokta sayısı"
    
    fig.text(0.77, 0.72, table_text,
            fontsize=9,
            verticalalignment='top',
            horizontalalignment='left',
            bbox=dict(boxstyle='round,pad=0.8', facecolor='lightblue', 
                     alpha=0.95, edgecolor='darkblue', linewidth=2.5),
            family='monospace',
            zorder=20)

# SAĞ ORTA - KESİŞİM DETAYLARI TABLOSU (Figure koordinatlarında) - YUKARI ALINIP GENİŞLETİLDİ
if distances:
    intersection_text = "KESİŞİM NOKTALARI DETAY\n" + "="*35 + "\n"
    intersection_text += "No   Açı    Mesafe(m)  Koord.\n"
    intersection_text += "-"*35 + "\n"
    
    # TÜM KESİŞİMLERİ LİSTELE
    for idx, (x, y, angle, dist, _) in enumerate(intersection_data):
        intersection_text += f"#{idx+1:2d}  {angle:4.0f}°   {dist:5.2f}m    ({x:5.2f},{y:5.2f})\n"
    
    fig.text(0.77, 0.50, intersection_text,
            fontsize=8.5,
            verticalalignment='top',
            horizontalalignment='left',
            bbox=dict(boxstyle='round,pad=0.8', facecolor='lightyellow', 
                     alpha=0.95, edgecolor='darkorange', linewidth=2.5),
            family='monospace',
            zorder=20)

# SAĞ ALTTA - İSTATİSTİKLER
if distances:
    stats_text = "İSTATİSTİKLER\n" + "="*25 + "\n\n"
    stats_text += f"En Yakın  : {np.min(distances):.2f}m\n"
    stats_text += f"En Uzak   : {np.max(distances):.2f}m\n"
    stats_text += f"Ortalama  : {np.mean(distances):.2f}m\n"
    stats_text += f"Std Sapma : {np.std(distances):.2f}m"
    
    fig.text(0.77, 0.12, stats_text,
            fontsize=9.5,
            verticalalignment='top',
            horizontalalignment='left',
            bbox=dict(boxstyle='round,pad=0.8', facecolor='lightcyan', 
                     alpha=0.95, edgecolor='darkblue', linewidth=2.5),
            family='monospace',
            zorder=20)

# Grafik ayarları
ax.grid(True, alpha=0.25, linestyle='--', linewidth=0.5)
ax.set_aspect('equal', adjustable='box')

# Grafik alanını ayarla - Bilgi kutuları için boşluk bırak
ax.set_xlim(x_min - margin, x_max + margin)
ax.set_ylim(y_min - margin, y_max + margin)

ax.axhline(y=0, color='k', linestyle='-', alpha=0.15, linewidth=1)
ax.axvline(x=0, color='k', linestyle='-', alpha=0.15, linewidth=1)

# Subplot pozisyonunu ayarla - Sağda ve üstte alan bırak
plt.subplots_adjust(left=0.05, right=0.75, top=0.92, bottom=0.08)

# Lejant - ALTTA GENİŞLETİLDİ
handles, labels = ax.get_legend_handles_labels()
basic_elements = []
basic_labels_list = []

if len(handles) >= 1:
    basic_elements.append(handles[0])
    basic_labels_list.append('LIDAR Noktalari')
if len(handles) >= 2:
    basic_elements.append(handles[1])
    basic_labels_list.append('Robot (0,0)')

# 60°+ kesişim ve mesafe çizgisi ekle
for i, label in enumerate(labels):
    if 'Kesisim' in label:
        basic_elements.append(handles[i])
        basic_labels_list.append('Kesisim (60°+)')
    if 'yakin' in label.lower():
        basic_elements.append(handles[i])
        basic_labels_list.append(labels[i])

ax.legend(basic_elements, basic_labels_list,
          loc='lower left', fontsize=10, framealpha=0.95, 
          ncol=2, handlelength=2,
          bbox_to_anchor=(0.01, 0.01),
          columnspacing=1.5,
          edgecolor='black',
          fancybox=True,
          shadow=True)

ax.set_title('LIDAR Veri Analizi ve Kesisim Tespiti', 
             fontsize=18, fontweight='bold', pad=20)
ax.set_xlabel('X (metre)', fontsize=14, fontweight='bold')
ax.set_ylabel('Y (metre)', fontsize=14, fontweight='bold')

# Grafik kaydet
if os.path.exists('lidar_plot.png'):
    os.remove('lidar_plot.png')

plt.savefig('lidar_plot.png', dpi=300, bbox_inches='tight', facecolor='white')
print("Grafik basariyla kaydedildi: lidar_plot.png")

if os.name == 'nt':
    os.system('start lidar_plot.png')