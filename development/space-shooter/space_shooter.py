import pygame
import serial
import time
import random

# --- Konfigurasi Serial Arduino ---
# GANTI 'COM3' (Windows) atau '/dev/ttyACM0' (Linux/Mac) sesuai port Arduino Anda
SERIAL_PORT = 'COM7' 
BAUD_RATE = 9600
JOYSTICK_CENTER = 512  # Nilai tengah sumbu X dari Arduino (sekitar 1024/2)
JOYSTICK_RANGE = 400   # Range untuk pergerakan
DEAD_ZONE = 50         # Area mati di sekitar titik tengah

try:
    # Inisialisasi koneksi serial
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    time.sleep(2) 
    print(f"Koneksi serial ke {SERIAL_PORT} berhasil.")
except serial.SerialException as e:
    print(f"Gagal menghubungkan ke port serial {SERIAL_PORT}: {e}")
    ser = None 

# --- Konfigurasi Pygame ---
pygame.init()

# Ukuran Layar
SCREEN_WIDTH = 800
SCREEN_HEIGHT = 600
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Arduino Space Shooter - V2")

# Warna
WHITE = (255, 255, 255)
RED = (255, 0, 0)
BLUE = (0, 0, 255)
BLACK = (0, 0, 0)

# Kecepatan Game
clock = pygame.time.Clock()
FPS = 60

# --- Variabel Game Global ---
score = 0
game_over = False

# --- Fungsi Utility ---
# Fungsi untuk menggambar teks (Score, Game Over)
font_name = pygame.font.match_font('arial')
def draw_text(surf, text, size, x, y):
    font = pygame.font.Font(font_name, size)
    text_surface = font.render(text, True, WHITE)
    text_rect = text_surface.get_rect()
    text_rect.midtop = (x, y)
    surf.blit(text_surface, text_rect)

# --- Kelas Game ---

# Kelas Pemain (Kapal)
class Player(pygame.sprite.Sprite):
    def __init__(self):
        super().__init__()
        self.image = pygame.Surface([50, 50])
        self.image.fill(BLUE)
        self.rect = self.image.get_rect()
        self.rect.centerx = SCREEN_WIDTH // 2
        self.rect.bottom = SCREEN_HEIGHT - 10
        self.speed = 5
        self.last_shot_time = pygame.time.get_ticks()
        self.fire_rate = 200 # Waktu jeda tembak (ms)

    # Fungsi update menerima input dari joystick
    def update(self, joystick_x=JOYSTICK_CENTER, fire_button=0):
        
        # 1. Kontrol Pergerakan dari Joystick
        deviation = joystick_x - JOYSTICK_CENTER
        
        if abs(deviation) > DEAD_ZONE: # Cek Dead Zone
            # Normalisasi deviasi untuk kecepatan halus
            normalized_deviation = deviation / JOYSTICK_RANGE
            self.rect.x += self.speed * normalized_deviation * 2 

        # Batasan layar
        if self.rect.right > SCREEN_WIDTH:
            self.rect.right = SCREEN_WIDTH
        if self.rect.left < 0:
            self.rect.left = 0

        # 2. Kontrol Tembak dari Joystick
        current_time = pygame.time.get_ticks()
        if fire_button == 1 and (current_time - self.last_shot_time) > self.fire_rate:
            self.shoot()
            self.last_shot_time = current_time

    def shoot(self):
        bullet = Bullet(self.rect.centerx, self.rect.top)
        all_sprites.add(bullet)
        bullets.add(bullet)

# Kelas Peluru
class Bullet(pygame.sprite.Sprite):
    def __init__(self, x, y):
        super().__init__()
        self.image = pygame.Surface([5, 15])
        self.image.fill(RED)
        self.rect = self.image.get_rect()
        self.rect.centerx = x
        self.rect.bottom = y
        self.speed = -10 # Bergerak ke atas

    def update(self):
        self.rect.y += self.speed
        if self.rect.bottom < 0:
            self.kill()

# NEW: Kelas Musuh
class Enemy(pygame.sprite.Sprite):
    def __init__(self):
        super().__init__()
        # Membuat musuh sederhana (segiempat merah)
        self.image = pygame.Surface([30, 30])
        self.image.fill(RED)
        self.rect = self.image.get_rect()
        
        # Posisi X acak di atas layar
        self.rect.x = random.randrange(0, SCREEN_WIDTH - self.rect.width)
        # Mulai sedikit di luar layar atas
        self.rect.y = random.randrange(-100, -40) 
        # Kecepatan acak agar variatif
        self.speed = random.randrange(1, 4) 

    def update(self):
        self.rect.y += self.speed
        # Jika musuh keluar dari layar bawah, reset posisinya (atau kill dan spawn baru)
        if self.rect.top > SCREEN_HEIGHT + 10:
            # Menggunakan kill() dan spawn_enemy() di main loop lebih bersih
            self.kill()


# --- Setup Game ---
all_sprites = pygame.sprite.Group()
bullets = pygame.sprite.Group()
enemies = pygame.sprite.Group() # Group baru untuk Musuh

player = Player()
all_sprites.add(player)

# Fungsi untuk spawn musuh
def spawn_enemy():
    e = Enemy()
    all_sprites.add(e)
    enemies.add(e)

# Inisialisasi 5 musuh awal
for _ in range(5):
    spawn_enemy()

# --- Loop Utama Game ---
running = True
joystick_x_value = JOYSTICK_CENTER
fire_button_state = 0 
last_spawn_time = pygame.time.get_ticks()
spawn_interval = 2000 # Musuh baru muncul setiap 2 detik

while running:
    
    # 1. Pemrosesan Input Serial (Arduino)
    if ser:
        try:
            line = ser.readline().decode('utf-8').strip()
            if line:
                parts = line.split(',')
                if len(parts) == 2:
                    try:
                        joystick_x_value = int(parts[0])
                        fire_button_state = int(parts[1])
                    except ValueError:
                        pass # Abaikan data yang tidak valid
        except Exception as e:
            print(f"Error membaca serial: {e}")
            
    # 2. Pemrosesan Event Pygame
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
            
    if game_over:
        # Tambahkan opsi untuk keluar saat Game Over
        keys = pygame.key.get_pressed()
        if keys[pygame.K_ESCAPE]:
            running = False
        continue # Lewati logika update game jika sudah Game Over

    # 3. Update Semua Sprite dan Logika Game
    player.update(joystick_x_value, fire_button_state)
    all_sprites.update()

    # Logika Spawn Musuh
    current_time = pygame.time.get_ticks()
    if current_time - last_spawn_time > spawn_interval:
        spawn_enemy()
        last_spawn_time = current_time

    # Logika Cek Musuh yang lolos/keluar layar
    for enemy in enemies:
        if enemy.rect.top > SCREEN_HEIGHT:
            enemy.kill() # Hapus musuh yang lolos
            # spawn_enemy() # Bisa di uncomment jika ingin musuh selalu ada 5

    # 4. Deteksi Tabrakan
    
    # Tabrakan Peluru vs Musuh
    # (True, True) menghapus musuh dan peluru yang bertabrakan
    hits = pygame.sprite.groupcollide(enemies, bullets, True, True)
    for hit in hits:
        # Tambahkan score saat musuh tertembak
        score += 10 
        # Spawn musuh baru untuk menjaga jumlah musuh
        spawn_enemy() 
        
    # Tabrakan Pemain vs Musuh
    # (True) menghapus musuh yang bertabrakan dengan pemain
    hits = pygame.sprite.spritecollide(player, enemies, True) 
    if hits:
        # Jika pemain tertabrak, Game Over
        game_over = True 

    
    # 5. Gambar / Render
    screen.fill(BLACK) 
    all_sprites.draw(screen) 
    
    # Gambar Score di bagian atas
    draw_text(screen, f"Score: {score}", 24, SCREEN_WIDTH / 2, 10)

    # Update tampilan
    pygame.display.flip()
    
    # Atur FPS
    clock.tick(FPS)

# --- Layar Game Over ---
if game_over:
    # Tampilkan layar Game Over setelah loop utama berakhir (jika terjadi game over)
    screen.fill(BLACK)
    draw_text(screen, "GAME OVER", 64, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 4)
    draw_text(screen, f"Final Score: {score}", 36, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2)
    draw_text(screen, "Tekan ESC untuk Keluar", 18, SCREEN_WIDTH / 2, SCREEN_HEIGHT * 3 / 4)
    pygame.display.flip()
    
    # Tunggu sebentar agar pengguna melihat layar Game Over
    while True:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                break
            if event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                running = False
                break
        if not running:
            break
        clock.tick(10) # FPS rendah untuk layar statis

# --- Keluar dari Game ---
if ser:
    ser.close() # Tutup koneksi serial
pygame.quit()
