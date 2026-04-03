// KenshiRotate Translation — Language detection and string lookup

#include "Translate.h"

#include <Debug.h>

#include <fstream>
#include <string>

// =====================================================
// Translation table
// =====================================================

enum LangId {
	LANG_EN, LANG_JA, LANG_ZH_CN, LANG_ZH_TW, LANG_KO,
	LANG_RU, LANG_DE, LANG_FR, LANG_ES, LANG_PT,
	LANG_COUNT
};

static const char* g_translations[LANG_COUNT][TR_COUNT] = {
	// LANG_EN — English
	{ "Middle Mouse", "Warning: bound to '", "'",
	  "Rotate key: ", "Press a key...",
	  "Middle mouse or keyboard. Esc to cancel.",
	  "Change", "Reset",
	  "Cannot rotate: item is equipped",
	  "Cannot rotate: item is square",
	  "Cannot rotate: save/load hooks failed",
	  "Cannot rotate: not enough space",
	  "None", "Secondary key: " },
	// LANG_JA — Japanese
	{ "\xe3\x83\x9e\xe3\x82\xa6\xe3\x82\xb9\xe4\xb8\xad\xe3\x83\x9c\xe3\x82\xbf\xe3\x83\xb3",                         // マウス中ボタン
	  "\xe8\xad\xa6\xe5\x91\x8a: '",                                                                                   // 警告: '
	  "' \xe3\x81\xab\xe5\x89\xb2\xe3\x82\x8a\xe5\xbd\x93\xe3\x81\xa6\xe6\xb8\x88\xe3\x81\xbf",                       // ' に割り当て済み
	  "\xe5\x9b\x9e\xe8\xbb\xa2\xe3\x82\xad\xe3\x83\xbc: ",                                                           // 回転キー:
	  "\xe3\x82\xad\xe3\x83\xbc\xe3\x82\x92\xe6\x8a\xbc\xe3\x81\x97\xe3\x81\xa6\xe3\x81\x8f\xe3\x81\xa0\xe3\x81\x95\xe3\x81\x84...", // キーを押してください...
	  "\xe3\x83\x9e\xe3\x82\xa6\xe3\x82\xb9\xe4\xb8\xad\xe3\x83\x9c\xe3\x82\xbf\xe3\x83\xb3\xe3\x81\xbe\xe3\x81\x9f\xe3\x81\xaf\xe3\x82\xad\xe3\x83\xbc\xe3\x83\x9c\xe3\x83\xbc\xe3\x83\x89\xe3\x80\x82" "Esc\xe3\x81\xa7\xe3\x82\xad\xe3\x83\xa3\xe3\x83\xb3\xe3\x82\xbb\xe3\x83\xab\xe3\x80\x82", // マウス中ボタンまたはキーボード。Escでキャンセル。
	  "\xe5\xa4\x89\xe6\x9b\xb4",                                                                                       // 変更
	  "\xe3\x83\xaa\xe3\x82\xbb\xe3\x83\x83\xe3\x83\x88",                                                              // リセット
	  "\xe5\x9b\x9e\xe8\xbb\xa2\xe3\x81\xa7\xe3\x81\x8d\xe3\x81\xbe\xe3\x81\x9b\xe3\x82\x93: \xe8\xa3\x85\xe5\x82\x99\xe4\xb8\xad\xe3\x81\xa7\xe3\x81\x99",                   // 回転できません: 装備中です
	  "\xe5\x9b\x9e\xe8\xbb\xa2\xe3\x81\xa7\xe3\x81\x8d\xe3\x81\xbe\xe3\x81\x9b\xe3\x82\x93: \xe6\xad\xa3\xe6\x96\xb9\xe5\xbd\xa2\xe3\x81\xa7\xe3\x81\x99",                   // 回転できません: 正方形です
	  "\xe5\x9b\x9e\xe8\xbb\xa2\xe3\x81\xa7\xe3\x81\x8d\xe3\x81\xbe\xe3\x81\x9b\xe3\x82\x93: \xe4\xbf\x9d\xe5\xad\x98" "/\xe8\xaa\xad\xe8\xbe\xbc\xe3\x83\x95\xe3\x83\x83\xe3\x82\xaf\xe5\xa4\xb1\xe6\x95\x97", // 回転できません: 保存/読込フック失敗
	  "\xe5\x9b\x9e\xe8\xbb\xa2\xe3\x81\xa7\xe3\x81\x8d\xe3\x81\xbe\xe3\x81\x9b\xe3\x82\x93: \xe3\x82\xb9\xe3\x83\x9a\xe3\x83\xbc\xe3\x82\xb9\xe4\xb8\x8d\xe8\xb6\xb3",   // 回転できません: スペース不足
	  "\xe3\x81\xaa\xe3\x81\x97",                                                                                           // なし
	  "\xe3\x82\xb5\xe3\x83\x96\xe3\x82\xad\xe3\x83\xbc: " },                                                              // サブキー:
	// LANG_ZH_CN — Chinese Simplified
	{ "\xe9\xbc\xa0\xe6\xa0\x87\xe4\xb8\xad\xe9\x94\xae",                                                             // 鼠标中键
	  "\xe8\xad\xa6\xe5\x91\x8a: \xe5\xb7\xb2\xe7\xbb\x91\xe5\xae\x9a\xe5\x88\xb0 '",                                 // 警告: 已绑定到 '
	  "'", "\xe6\x97\x8b\xe8\xbd\xac\xe6\x8c\x89\xe9\x94\xae: ",                                                       // 旋转按键:
	  "\xe8\xaf\xb7\xe6\x8c\x89\xe4\xb8\x8b\xe6\x8c\x89\xe9\x94\xae...",                                               // 请按下按键...
	  "\xe9\xbc\xa0\xe6\xa0\x87\xe4\xb8\xad\xe9\x94\xae\xe6\x88\x96\xe9\x94\xae\xe7\x9b\x98\xe3\x80\x82" "Esc \xe5\x8f\x96\xe6\xb6\x88\xe3\x80\x82", // 鼠标中键或键盘。Esc 取消。
	  "\xe6\x9b\xb4\xe6\x94\xb9",                                                                                       // 更改
	  "\xe9\x87\x8d\xe7\xbd\xae",                                                                                       // 重置
	  "\xe6\x97\xa0\xe6\xb3\x95\xe6\x97\x8b\xe8\xbd\xac: \xe7\x89\xa9\xe5\x93\x81\xe5\xb7\xb2\xe8\xa3\x85\xe5\xa4\x87",                                 // 无法旋转: 物品已装备
	  "\xe6\x97\xa0\xe6\xb3\x95\xe6\x97\x8b\xe8\xbd\xac: \xe7\x89\xa9\xe5\x93\x81\xe6\x98\xaf\xe6\xad\xa3\xe6\x96\xb9\xe5\xbd\xa2",                     // 无法旋转: 物品是正方形
	  "\xe6\x97\xa0\xe6\xb3\x95\xe6\x97\x8b\xe8\xbd\xac: \xe4\xbf\x9d\xe5\xad\x98" "/\xe5\x8a\xa0\xe8\xbd\xbd\xe9\x92\xa9\xe5\xad\x90\xe5\xa4\xb1\xe8\xb4\xa5", // 无法旋转: 保存/加载钩子失败
	  "\xe6\x97\xa0\xe6\xb3\x95\xe6\x97\x8b\xe8\xbd\xac: \xe7\xa9\xba\xe9\x97\xb4\xe4\xb8\x8d\xe8\xb6\xb3",                                           // 无法旋转: 空间不足
	  "\xe6\x97\xa0",                                                                                                       // 无
	  "\xe5\x89\xaf\xe9\x94\xae: " },                                                                                       // 副键:
	// LANG_ZH_TW — Chinese Traditional
	{ "\xe6\xbb\x91\xe9\xbc\xa0\xe4\xb8\xad\xe9\x8d\xb5",                                                             // 滑鼠中鍵
	  "\xe8\xad\xa6\xe5\x91\x8a: \xe5\xb7\xb2\xe7\xb6\x81\xe5\xae\x9a\xe5\x88\xb0 '",                                 // 警告: 已綁定到 '
	  "'", "\xe6\x97\x8b\xe8\xbd\x89\xe6\x8c\x89\xe9\x8d\xb5: ",                                                       // 旋轉按鍵:
	  "\xe8\xab\x8b\xe6\x8c\x89\xe4\xb8\x8b\xe6\x8c\x89\xe9\x8d\xb5...",                                               // 請按下按鍵...
	  "\xe6\xbb\x91\xe9\xbc\xa0\xe4\xb8\xad\xe9\x8d\xb5\xe6\x88\x96\xe9\x8d\xb5\xe7\x9b\xa4\xe3\x80\x82" "Esc \xe5\x8f\x96\xe6\xb6\x88\xe3\x80\x82", // 滑鼠中鍵或鍵盤。Esc 取消。
	  "\xe8\xae\x8a\xe6\x9b\xb4",                                                                                       // 變更
	  "\xe9\x87\x8d\xe8\xa8\xad",                                                                                       // 重設
	  "\xe7\x84\xa1\xe6\xb3\x95\xe6\x97\x8b\xe8\xbd\x89: \xe7\x89\xa9\xe5\x93\x81\xe5\xb7\xb2\xe8\xa3\x9d\xe5\x82\x99",                                 // 無法旋轉: 物品已裝備
	  "\xe7\x84\xa1\xe6\xb3\x95\xe6\x97\x8b\xe8\xbd\x89: \xe7\x89\xa9\xe5\x93\x81\xe6\x98\xaf\xe6\xad\xa3\xe6\x96\xb9\xe5\xbd\xa2",                     // 無法旋轉: 物品是正方形
	  "\xe7\x84\xa1\xe6\xb3\x95\xe6\x97\x8b\xe8\xbd\x89: \xe5\x84\xb2\xe5\xad\x98" "/\xe8\xbc\x89\xe5\x85\xa5\xe6\x8e\x9b\xe9\x89\xa4\xe5\xa4\xb1\xe6\x95\x97", // 無法旋轉: 儲存/載入掛鉤失敗
	  "\xe7\x84\xa1\xe6\xb3\x95\xe6\x97\x8b\xe8\xbd\x89: \xe7\xa9\xba\xe9\x96\x93\xe4\xb8\x8d\xe8\xb6\xb3",                                           // 無法旋轉: 空間不足
	  "\xe7\x84\xa1",                                                                                                       // 無
	  "\xe5\x89\xaf\xe9\x8d\xb5: " },                                                                                       // 副鍵:
	// LANG_KO — Korean
	{ "\xeb\xa7\x88\xec\x9a\xb0\xec\x8a\xa4 \xea\xb0\x80\xec\x9a\xb4\xeb\x8d\xb0 \xeb\xb2\x84\xed\x8a\xbc",           // 마우스 가운데 버튼
	  "\xea\xb2\xbd\xea\xb3\xa0: '",                                                                                   // 경고: '
	  "'\xec\x97\x90 \xed\x95\xa0\xeb\x8b\xb9\xeb\x90\xa8",                                                            // '에 할당됨
	  "\xed\x9a\x8c\xec\xa0\x84 \xed\x82\xa4: ",                                                                       // 회전 키:
	  "\xed\x82\xa4\xeb\xa5\xbc \xeb\x88\x84\xeb\xa5\xb4\xec\x84\xb8\xec\x9a\x94...",                                   // 키를 누르세요...
	  "\xeb\xa7\x88\xec\x9a\xb0\xec\x8a\xa4 \xea\xb0\x80\xec\x9a\xb4\xeb\x8d\xb0 \xeb\xb2\x84\xed\x8a\xbc \xeb\x98\x90\xeb\x8a\x94 \xed\x82\xa4\xeb\xb3\xb4\xeb\x93\x9c. Esc\xeb\xa1\x9c \xec\xb7\xa8\xec\x86\x8c.", // 마우스 가운데 버튼 또는 키보드. Esc로 취소.
	  "\xeb\xb3\x80\xea\xb2\xbd",                                                                                       // 변경
	  "\xec\xb4\x88\xea\xb8\xb0\xed\x99\x94",                                                                          // 초기화
	  "\xed\x9a\x8c\xec\xa0\x84 \xeb\xb6\x88\xea\xb0\x80: \xec\x95\x84\xec\x9d\xb4\xed\x85\x9c \xec\x9e\xa5\xec\xb0\xa9 \xec\xa4\x91",                 // 회전 불가: 아이템 장착 중
	  "\xed\x9a\x8c\xec\xa0\x84 \xeb\xb6\x88\xea\xb0\x80: \xec\xa0\x95\xec\x82\xac\xea\xb0\x81\xed\x98\x95 \xec\x95\x84\xec\x9d\xb4\xed\x85\x9c",                           // 회전 불가: 정사각형 아이템
	  "\xed\x9a\x8c\xec\xa0\x84 \xeb\xb6\x88\xea\xb0\x80: \xec\xa0\x80\xec\x9e\xa5/\xeb\xb6\x88\xeb\x9f\xac\xec\x98\xa4\xea\xb8\xb0 \xed\x9b\x85 \xec\x8b\xa4\xed\x8c\xa8", // 회전 불가: 저장/불러오기 훅 실패
	  "\xed\x9a\x8c\xec\xa0\x84 \xeb\xb6\x88\xea\xb0\x80: \xea\xb3\xb5\xea\xb0\x84 \xeb\xb6\x80\xec\xa1\xb1",                                         // 회전 불가: 공간 부족
	  "\xec\x97\x86\xec\x9d\x8c",                                                                                           // 없음
	  "\xeb\xb3\xb4\xec\xa1\xb0 \xed\x82\xa4: " },                                                                         // 보조 키:
	// LANG_RU — Russian
	{ "\xd0\xa1\xd1\x80\xd0\xb5\xd0\xb4\xd0\xbd\xd1\x8f\xd1\x8f \xd0\xba\xd0\xbd\xd0\xbe\xd0\xbf\xd0\xba\xd0\xb0 \xd0\xbc\xd1\x8b\xd1\x88\xd0\xb8", // Средняя кнопка мыши
	  "\xd0\x92\xd0\xbd\xd0\xb8\xd0\xbc\xd0\xb0\xd0\xbd\xd0\xb8\xd0\xb5: \xd0\xbd\xd0\xb0\xd0\xb7\xd0\xbd\xd0\xb0\xd1\x87\xd0\xb5\xd0\xbd\xd0\xbe \xd0\xbd\xd0\xb0 '", // Внимание: назначено на '
	  "'", "\xd0\x9a\xd0\xbb\xd0\xb0\xd0\xb2\xd0\xb8\xd1\x88\xd0\xb0 \xd0\xb2\xd1\x80\xd0\xb0\xd1\x89\xd0\xb5\xd0\xbd\xd0\xb8\xd1\x8f: ", // Клавиша вращения:
	  "\xd0\x9d\xd0\xb0\xd0\xb6\xd0\xbc\xd0\xb8\xd1\x82\xd0\xb5 \xd0\xba\xd0\xbb\xd0\xb0\xd0\xb2\xd0\xb8\xd1\x88\xd1\x83...", // Нажмите клавишу...
	  "\xd0\xa1\xd1\x80\xd0\xb5\xd0\xb4\xd0\xbd\xd1\x8f\xd1\x8f \xd0\xba\xd0\xbd\xd0\xbe\xd0\xbf\xd0\xba\xd0\xb0 \xd0\xbc\xd1\x8b\xd1\x88\xd0\xb8 \xd0\xb8\xd0\xbb\xd0\xb8 \xd0\xba\xd0\xbb\xd0\xb0\xd0\xb2\xd0\xb8\xd0\xb0\xd1\x82\xd1\x83\xd1\x80\xd0\xb0. Esc \xd0\xb4\xd0\xbb\xd1\x8f \xd0\xbe\xd1\x82\xd0\xbc\xd0\xb5\xd0\xbd\xd1\x8b.", // Средняя кнопка мыши или клавиатура. Esc для отмены.
	  "\xd0\x98\xd0\xb7\xd0\xbc\xd0\xb5\xd0\xbd\xd0\xb8\xd1\x82\xd1\x8c",                                             // Изменить
	  "\xd0\xa1\xd0\xb1\xd1\x80\xd0\xbe\xd1\x81",                                                                      // Сброс
	  "\xd0\x9d\xd0\xb5\xd0\xbb\xd1\x8c\xd0\xb7\xd1\x8f \xd0\xb2\xd1\x80\xd0\xb0\xd1\x89\xd0\xb0\xd1\x82\xd1\x8c: \xd0\xbf\xd1\x80\xd0\xb5\xd0\xb4\xd0\xbc\xd0\xb5\xd1\x82 \xd1\x8d\xd0\xba\xd0\xb8\xd0\xbf\xd0\xb8\xd1\x80\xd0\xbe\xd0\xb2\xd0\xb0\xd0\xbd", // Нельзя вращать: предмет экипирован
	  "\xd0\x9d\xd0\xb5\xd0\xbb\xd1\x8c\xd0\xb7\xd1\x8f \xd0\xb2\xd1\x80\xd0\xb0\xd1\x89\xd0\xb0\xd1\x82\xd1\x8c: \xd0\xbf\xd1\x80\xd0\xb5\xd0\xb4\xd0\xbc\xd0\xb5\xd1\x82 \xd0\xba\xd0\xb2\xd0\xb0\xd0\xb4\xd1\x80\xd0\xb0\xd1\x82\xd0\xbd\xd1\x8b\xd0\xb9",                                   // Нельзя вращать: предмет квадратный
	  "\xd0\x9d\xd0\xb5\xd0\xbb\xd1\x8c\xd0\xb7\xd1\x8f \xd0\xb2\xd1\x80\xd0\xb0\xd1\x89\xd0\xb0\xd1\x82\xd1\x8c: \xd0\xbe\xd1\x88\xd0\xb8\xd0\xb1\xd0\xba\xd0\xb0 \xd1\x85\xd1\x83\xd0\xba\xd0\xbe\xd0\xb2 \xd1\x81\xd0\xbe\xd1\x85\xd1\x80\xd0\xb0\xd0\xbd\xd0\xb5\xd0\xbd\xd0\xb8\xd1\x8f/\xd0\xb7\xd0\xb0\xd0\xb3\xd1\x80\xd1\x83\xd0\xb7\xd0\xba\xd0\xb8", // Нельзя вращать: ошибка хуков сохранения/загрузки
	  "\xd0\x9d\xd0\xb5\xd0\xbb\xd1\x8c\xd0\xb7\xd1\x8f \xd0\xb2\xd1\x80\xd0\xb0\xd1\x89\xd0\xb0\xd1\x82\xd1\x8c: \xd0\xbd\xd0\xb5\xd0\xb4\xd0\xbe\xd1\x81\xd1\x82\xd0\xb0\xd1\x82\xd0\xbe\xd1\x87\xd0\xbd\xd0\xbe \xd0\xbc\xd0\xb5\xd1\x81\xd1\x82\xd0\xb0",                               // Нельзя вращать: недостаточно места
	  "\xd0\x9d\xd0\xb5\xd1\x82",                                                                                           // Нет
	  "\xd0\x94\xd0\xbe\xd0\xbf. \xd0\xba\xd0\xbb\xd0\xb0\xd0\xb2\xd0\xb8\xd1\x88\xd0\xb0: " },                           // Доп. клавиша:
	// LANG_DE — German
	{ "Mittlere Maustaste", "Warnung: belegt mit '", "'",
	  "Drehtaste: ", "Taste dr\xc3\xbc" "cken...",                                                                      // Taste drücken...
	  "Mittlere Maustaste oder Tastatur. Esc zum Abbrechen.",
	  "\xc3\x84ndern",                                                                                                   // Ändern
	  "Zur\xc3\xbc" "cksetzen",                                                                                         // Zurücksetzen
	  "Drehen nicht m\xc3\xb6glich: Gegenstand ist ausger\xc3\xbcstet",                                                  // Drehen nicht möglich: Gegenstand ist ausgerüstet
	  "Drehen nicht m\xc3\xb6glich: Gegenstand ist quadratisch",                                                        // Drehen nicht möglich: Gegenstand ist quadratisch
	  "Drehen nicht m\xc3\xb6glich: Speicher-/Ladehooks fehlgeschlagen",                                                // Drehen nicht möglich: Speicher-/Ladehooks fehlgeschlagen
	  "Drehen nicht m\xc3\xb6glich: nicht genug Platz",
	  "Keine", "Zweittaste: " },                                                               // Drehen nicht möglich: nicht genug Platz
	// LANG_FR — French
	{ "Clic molette", "Attention : assign\xc3\xa9 \xc3\xa0 '",                                                          // Attention : assigné à '
	  "'", "Touche de rotation : ",
	  "Appuyez sur une touche...",
	  "Clic molette ou clavier. \xc3\x89" "chap pour annuler.",                                                          // Échap pour annuler.
	  "Modifier",
	  "R\xc3\xa9initialiser",                                                                                            // Réinitialiser
	  "Rotation impossible : objet \xc3\xa9quip\xc3\xa9",                                                                // Rotation impossible : objet équipé
	  "Rotation impossible : objet carr\xc3\xa9",                                                                        // Rotation impossible : objet carré
	  "Rotation impossible : \xc3\xa9" "chec des hooks de sauvegarde/chargement",                                        // Rotation impossible : échec des hooks de sauvegarde/chargement
	  "Rotation impossible : espace insuffisant",
	  "Aucune", "Touche secondaire : " },
	// LANG_ES — Spanish
	{ "Bot\xc3\xb3n central del rat\xc3\xb3n",                                                                          // Botón central del ratón
	  "Aviso: asignado a '", "'",
	  "Tecla de rotaci\xc3\xb3n: ",                                                                                      // Tecla de rotación:
	  "Pulsa una tecla...",
	  "Bot\xc3\xb3n central del rat\xc3\xb3n o teclado. Esc para cancelar.",                                            // Botón central del ratón o teclado. Esc para cancelar.
	  "Cambiar",
	  "Restablecer",
	  "No se puede rotar: objeto equipado",
	  "No se puede rotar: objeto cuadrado",
	  "No se puede rotar: fallo en los hooks de guardado/carga",
	  "No se puede rotar: espacio insuficiente",
	  "Ninguna", "Tecla secundaria: " },
	// LANG_PT — Portuguese
	{ "Bot\xc3\xa3o do meio do mouse",                                                                                  // Botão do meio do mouse
	  "Aviso: atribu\xc3\xad" "do a '",                                                                                 // Aviso: atribuído a '
	  "'", "Tecla de rota\xc3\xa7\xc3\xa3o: ",                                                                          // Tecla de rotação:
	  "Pressione uma tecla...",
	  "Bot\xc3\xa3o do meio do mouse ou teclado. Esc para cancelar.",                                                   // Botão do meio do mouse ou teclado. Esc para cancelar.
	  "Alterar",
	  "Redefinir",
	  "N\xc3\xa3o \xc3\xa9 poss\xc3\xadvel rotacionar: item equipado",                                                 // Não é possível rotacionar: item equipado
	  "N\xc3\xa3o \xc3\xa9 poss\xc3\xadvel rotacionar: item quadrado",                                                  // Não é possível rotacionar: item quadrado
	  "N\xc3\xa3o \xc3\xa9 poss\xc3\xadvel rotacionar: falha nos hooks de salvar/carregar",                            // Não é possível rotacionar: falha nos hooks de salvar/carregar
	  "N\xc3\xa3o \xc3\xa9 poss\xc3\xadvel rotacionar: espa\xc3\xa7o insuficiente",
	  "Nenhuma", "Tecla secund\xc3\xa1ria: " }                                   // Não é possível rotacionar: espaço insuficiente
};

static LangId g_currentLang = LANG_EN;

// =====================================================
// Public API
// =====================================================

const char* Tr(TrKey key)
{
	return g_translations[g_currentLang][key];
}

void DetectLanguage()
{
	std::ifstream file("settings.cfg");
	if (!file.is_open())
	{
		DebugLog("[KenshiRotate] Could not open settings.cfg, defaulting to English");
		return;
	}

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#' || line[0] == '[')
			continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		std::string k = line.substr(0, eq);
		if (k != "language")
			continue;

		std::string lang = line.substr(eq + 1);
		DebugLog("[KenshiRotate] Detected language: " + lang);

		if (lang.substr(0, 2) == "ja")      g_currentLang = LANG_JA;
		else if (lang.substr(0, 5) == "zh_CN" ||
		         lang.substr(0, 5) == "zh_SG") g_currentLang = LANG_ZH_CN;
		else if (lang.substr(0, 5) == "zh_TW" ||
		         lang.substr(0, 5) == "zh_HK") g_currentLang = LANG_ZH_TW;
		else if (lang.substr(0, 2) == "ko")  g_currentLang = LANG_KO;
		else if (lang.substr(0, 2) == "ru")  g_currentLang = LANG_RU;
		else if (lang.substr(0, 2) == "de")  g_currentLang = LANG_DE;
		else if (lang.substr(0, 2) == "fr")  g_currentLang = LANG_FR;
		else if (lang.substr(0, 2) == "es")  g_currentLang = LANG_ES;
		else if (lang.substr(0, 2) == "pt")  g_currentLang = LANG_PT;
		return;
	}
}
