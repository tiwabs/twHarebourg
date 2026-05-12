# twHarebourg

Outil overlay pour le combat contre le boss **Harebourg** dans Dofus.

Il se superpose par-dessus le jeu (toujours au premier plan, fond transparent) et indique la cellule depuis laquelle lancer le sort **Confusion** pour que la téléportation ennemie atterrisse au bon endroit.  
En mode Placement **ON**, l'overlay capture les clics souris pour interagir avec la grille et le HUD. En mode Placement **OFF**, les clics passent directement au jeu. Tous les raccourcis clavier sont globaux et fonctionnent dans les deux modes.

---

## Utilisation pas à pas

1. **Lancez `twHarebourg.exe`** — l'overlay couvre tout l'écran principal, la grille de combat apparaît en surimpression.
2. **Activez le mode Placement** avec `F2` (le HUD affiche `Mode placement : ON`).  
   En mode Placement **ON**, vos clics sont capturés par l'overlay : vous pouvez cliquer sur la grille et interagir avec le HUD.
3. **Désactivez le mode Placement** (`F2` à nouveau, `Mode placement : OFF`) pour que les clics passent à travers l'overlay directement vers le jeu.
4. **Clic gauche** sur une cellule → place votre personnage (cellule bleue).  
   **Clic droit** sur une cellule → place la cible / le boss (cellule rouge).
5. **Ajustez les PV (%)** du joueur via le slider `HP (%)` dans le HUD.
6. **Comptez les coups de mêlée** infligés depuis le début de la phase et incrémentez/décrémentez avec `PageUp` / `PageDown`.
7. La **cellule suggérée** (cyan) s'affiche automatiquement : c'est la cellule depuis laquelle vous devez lancer Confusion. Une ligne cyan relie votre position à cette cellule.

> Si aucune cellule n'est suggérée, soit les positions ne sont pas encore définies, soit aucune cellule valide (accessible et en ligne de vue) n'existe pour la rotation calculée.

---

## Code couleur de la grille

| Couleur | Signification |
|---------|---------------|
| Bleu | Position du joueur |
| Rouge | Position de la cible (boss) |
| Cyan / bleu clair | Cellule suggérée pour lancer Confusion |
| Grisé / assombri | Cellules hors ligne de vue depuis le joueur |

---

## Calcul de la rotation de Confusion

La rotation dépend de deux paramètres :

### 1. Rotation de base (PV du joueur)

| PV du joueur | Rotation de base |
|------------|-----------------|
| 90 – 100 % | +90° (horaire) |
| 75 – 89 %  | −90° (contre-horaire) |
| 45 – 74 %  | 180° |
| 30 – 44 %  | −90° (contre-horaire) |
| 0 – 29 %   | +90° (horaire) |

### 2. Stacks de mêlée

Chaque coup de mêlée infligé décale la rotation de **+90°** supplémentaires (cumulatif).  
Rotation finale = rotation de base + stacks × 90°.

L'outil applique ensuite cette rotation au vecteur joueur → cible pour déterminer la cellule optimale.

---

## Raccourcis clavier

Tous les raccourcis sont **globaux** (actifs même quand Dofus a le focus).

| Touche | Action |
|--------|--------|
| `F2` | Activer / désactiver le mode Placement — ON : clics capturés par l'overlay (grille/HUD) ; OFF : clics passent au jeu |
| `PageUp` | Ajouter un stack de mêlée (+1) |
| `PageDown` | Retirer un stack de mêlée (−1) |
| `Suppr` | Réinitialiser les stacks de mêlée à 0 |
| `F8` | Activer / désactiver le mode Édition de grille |
| `F9` | Sauvegarder la configuration de la grille dans `config.json` |
| `F10` | Quitter |

### Raccourcis supplémentaires en mode Édition de grille (`F8`)

| Touche | Action |
|--------|--------|
| `←` `→` `↑` `↓` | Déplacer l'origine de la grille (4 px par pression) |
| `Shift` + flèches | Déplacer l'origine finement (1 px par pression) |
| Molette haut/bas | Agrandir / réduire la taille des tuiles (2 px par cran) |
| `Shift` + molette | Ajustement fin de la taille des tuiles (1 px par cran) |

---

## Configuration

Le fichier `config.json` (dans le même dossier que l'exe) contrôle le positionnement de la grille sur l'écran. Il est mis à jour automatiquement via `F9` après une édition en mode grille.

```json
{
  "base_width": 1920,
  "base_height": 1080,
  "grid_origin_x": 932,
  "grid_origin_y": 92,
  "tile_w": 92,
  "tile_h": 46
}
```

| Clé | Description |
|-----|-------------|
| `base_width` / `base_height` | Résolution de référence pour laquelle les coordonnées ont été calibrées. La grille est mise à l'échelle automatiquement si votre résolution diffère. |
| `grid_origin_x` / `grid_origin_y` | Position en pixels de la cellule (0, 0) de la grille dans la résolution de référence. |
| `tile_w` / `tile_h` | Largeur et hauteur d'une tuile en pixels (rapport 2:1 pour l'isométrique). |

Si votre résolution ou disposition diffère de 1920×1080, utilisez le **mode Édition** (`F8`) pour recalibrer visuellement la grille, puis sauvegardez avec `F9`.

---

## Build (développeurs)

**Prérequis :** MSYS2 avec les paquets `mingw-w64-x86_64-glfw` et `cmake`.

```bat
build.bat
```

Le build produit un exécutable Release statiquement lié dans `build/twHarebourg.exe`.

---

## Licence

MIT — voir [LICENSE](LICENSE).
