//
// Created by Антон Чемодуров on 30.11.2021.
//
#include "bmp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int creat_from_bmp(const BMP *bmp, const char *file_name) {
    FILE *bmp_file = fopen(file_name, "wb");
    if (bmp_file == NULL) {
        return -1;
    }
    size_t written = fwrite(bmp->header, 1,  sizeof(bmp_file_header), bmp_file);
    if (written != sizeof(bmp_file_header)) {
        fclose(bmp_file);
        return -1;
    }
    size_t info_size = bmp->header->bfOffBits - 14;
    if (bmp->info_header != NULL) {
        written = fwrite(bmp->info_header, 1, info_size, bmp_file);
    } else {
        written = fwrite(bmp->info_core_header, 1, info_size, bmp_file);
    }
    if (written != info_size) {
        fclose(bmp_file);
        return -1;
    }
    size_t bitmap_size = bmp->header->bfSize - bmp->header->bfOffBits;
    written = fwrite(bmp->bitmap, 1, bitmap_size, bmp_file);
    if (written != bitmap_size) {
        fclose(bmp_file);
        return -1;
    }
    fclose(bmp_file);
    return 0;
}

int creat_from_data(uint16_t width, uint16_t height,
                    char *bitmap, size_t bitmap_size, const char *file_name) {
    bmp_file_header header = {
            BMP_TYPE,
            BMP_HEADER_S + BMP_CORE_HEADER_S + BMP_CORE_COLORTab_S + bitmap_size,
            0,
            0,
            BMP_HEADER_S + BMP_CORE_HEADER_S + BMP_CORE_COLORTab_S
    };
    /**
     * черный/белый цевета
     * в creat_from_bmp будет переполение буффера
     * мы копируем 18 байт, а размер bmp_core_header 12
     * поэтому мы захватим первые 6 байт color
     */
    uint64_t color = 0xffffff000000;
    bmp_core_header info_core_header = {
        BMP_CORE_HEADER_S,
        width,
        height,
        1,
        1
    };
    BMP bmp = {
            &header,
            &info_core_header,
            NULL,
            bitmap
    };
    return creat_from_bmp(&bmp, file_name);
}

/**
 * функция парсит bmp файл
 *
 */
BMP *open(const char *file_name) {
    FILE *bmp_file = fopen(file_name, "rb");
    if (bmp_file == NULL) {
        perror("error: ");
        return NULL;
    }
    bmp_file_header *header = malloc(BMP_HEADER_S); // выделяем память под заголовок

    // считаем размер файла
    fseek(bmp_file, 0, SEEK_END); // передвигаем указатель в конец
    uint32_t file_size = ftell(bmp_file); // возвращаем количество байт с текущей позиции указателя до начала файла
    rewind(bmp_file); // возвращаем указатель в начала файла

    /**
     * читаем заголовок сразу в структуру
     * можем так делать потому что указали выравнивание в 1 байт
     */
    size_t readed = fread(header, BMP_HEADER_S, 1, bmp_file);
    /**
     * выдкляем помять под буфер для информационного заголовка + таблицы цветов
     */
    size_t buff_size = header->bfOffBits - BMP_HEADER_S; // начало пикисильных данных - 14 (размер заголовка)
    char *buffer = malloc(buff_size);

    if (readed != 1 ||
        header->bfType != BMP_TYPE ||
        header->bfSize != file_size) // проверяем корректно или мы считали заголовк и bmp ли это
    {
        goto failed;
    }
    // читаем  информационный заголовока + таблица цветов
    readed = fread(buffer, 1, buff_size, bmp_file);

    if (readed != buff_size) {
        goto failed;
    }

    bmp_core_header *info_core_header = NULL;
    bmp_info_header  *info_header = NULL;
    uint32_t version = *(uint32_t *)buffer; // определяем версию заголовка (первые 4 байта)
    /**
     * что такое info_**_header
     * по сути это участок памяти хранящий
     * информационный заголовок + таблицу цветов
     * к первым х бацтам мы можем обращатся через структуру
     * так как они нужны для реализации игры жизнь
     * остальный мы просто храним, чтобы потом без
     * лишней вознь создать новый bmp файл с такими же
     * заголовками но с другими пискильными данными, так как игра жизнь меняет
     * только значение пиксельных данных, а не размер глубину цвета и тд
     */
    if (version == 12) {
        info_core_header = (bmp_core_header* )buffer;
        if (info_core_header->bcBitCount != 1 ||
            !info_core_header->bcWidth ||
            !info_core_header->bcHeight)
        {
            goto failed;
        }
    } else {
        info_header = (bmp_info_header *)buffer;
        if (info_header->biBitCount != 1 ||
                !info_header->biWidth ||
                !info_header->biHeight)
        {
            goto failed;
        }
    }
    /**
     * данные в мнохоромном bmp файле хранятся в виде друмерного массива БИТ
     * так как процессор не умеет оперировать битами по отдельности
     * в дальшем этот массив нужно будет "распокавать"
     * я представлял его в виде массива байт, где каждый БАЙТ либо 0 либо 1
     */
    buff_size = file_size - header->bfOffBits; // размер массива данных
    char *bitmap = malloc(buff_size);
    readed = fread(bitmap, 1, buff_size, bmp_file);
    fclose(bmp_file);
    if (readed != buff_size) {
        free(header);
        free(buffer);
        free(bitmap);
        return NULL;
    }
    BMP *bmp = malloc(sizeof(BMP));
    bmp->info_header = info_header;
    bmp->info_core_header = info_core_header;
    bmp->bitmap = bitmap;
    bmp->header = header;
    return bmp;

    failed:
        free(header);
        free(buffer);
        fclose(bmp_file);
        return NULL;
}





void delete(BMP *bmp) {
    free(bmp->info_header);
    free(bmp->info_core_header);
    free(bmp->bitmap);
    free(bmp->header);
    free(bmp);
}

uint32_t get_BMP_width(BMP *bmp) {
    if (bmp->info_header != NULL) {
        return bmp->info_header->biWidth;
    } else {
        return bmp->info_core_header->bcWidth;
    }
}

uint32_t get_BMP_height(BMP *bmp) {
    if (bmp->info_header != NULL) {
        return bmp->info_header->biHeight;
    } else {
        return bmp->info_core_header->bcHeight;
    }
}

uint32_t get_data_size(BMP *bmp) {
    return bmp->header->bfSize - bmp->header->bfOffBits;
}

