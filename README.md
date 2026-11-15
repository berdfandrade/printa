# printa

Captura de tela simples em C + GTK3, no estilo do Flameshot. Só funciona em X11.

## Compilar

```bash
sudo apt install libgtk-3-dev
make
```

## Usar

```bash
./printa          # captura agora
./printa -d 3     # espera 3 s (útil para capturar menus abertos)
```

A tela congela e escurece:

- **passe o mouse** sobre uma janela para destacá-la; **clique** para selecioná-la
- **arraste** para selecionar uma região livre
- **Enter** salva em `~/Imagens/Captura_<data>.png` e copia para a área de transferência
  (sem seleção, captura a tela inteira); o botão **Salvar**, que aparece junto à
  seleção, e o **duplo clique** numa janela fazem o mesmo
- **Ctrl+C** só copia · **Ctrl+S** abre "Salvar como" (`.png` ou `.jpg`)
- **Esc** ou **botão direito** cancela

O caminho do arquivo salvo é impresso no stdout. A cópia continua disponível
depois que o programa fecha se houver um gerenciador de área de transferência
(o Cinnamon tem um).

Para usar com Print Screen: em Configurações → Teclado → Atalhos, crie um atalho
personalizado apontando para o caminho do `printa`.

## Licença

MIT — veja [LICENSE](LICENSE).
