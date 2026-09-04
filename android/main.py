"""
NC Terminal - app Android minimo (tela preta, letras brancas) que roda
codigo NC Language chamando o binario nativo empacotado (libnclang.so).

© 2026 NWL-Systems
"""
import os
import subprocess

from kivy.app import App
from kivy.core.window import Window
from kivy.metrics import dp, sp
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.label import Label
from kivy.uix.scrollview import ScrollView
from kivy.uix.textinput import TextInput

Window.clearcolor = (0, 0, 0, 1)  # fundo preto


def native_lib_dir():
    """
    Pasta onde o Android extrai os .so do app (unica pasta onde o sistema
    deixa executar binarios, a partir do Android 10). Fora do Android
    (rodando o main.py direto no PC pra testar a UI), cai pra pasta local.
    """
    try:
        from jnius import autoclass
        PythonActivity = autoclass("org.kivy.android.PythonActivity")
        return PythonActivity.mActivity.getApplicationInfo().nativeLibraryDir
    except Exception:
        return os.path.join(os.path.dirname(os.path.abspath(__file__)), "libs", "arm64-v8a")


NCLANG_PATH = os.path.join(native_lib_dir(), "libnclang.so")


class Terminal(BoxLayout):
    def __init__(self, **kwargs):
        super().__init__(orientation="vertical", **kwargs)
        self.sessao = ""  # historico da sessao (mesma logica do nc-cmd de terminal)

        self.output = Label(
            text="=== NC Terminal ===\n"
                 "Digite um comando NC e aperte Enter.\n"
                 "'reset' limpa as variaveis. 'clear' limpa a tela.\n\n",
            color=(1, 1, 1, 1),
            font_size=sp(13),
            size_hint_y=None,
            halign="left",
            valign="top",
            markup=False,
        )
        self.output.bind(
            texture_size=lambda inst, size: setattr(self.output, "height", size[1])
        )
        Window.bind(on_resize=self._resize_text)
        self._resize_text(Window, Window.width, Window.height)

        self.scroll = ScrollView(size_hint=(1, 0.88))
        self.scroll.add_widget(self.output)
        self.add_widget(self.scroll)

        self.entrada = TextInput(
            multiline=False,
            background_color=(0, 0, 0, 1),
            foreground_color=(1, 1, 1, 1),
            cursor_color=(1, 1, 1, 1),
            font_size=sp(13),
            size_hint=(1, 0.12),
            hint_text="nc> ",
            write_tab=False,
        )
        self.entrada.bind(on_text_validate=self.roda_comando)
        self.add_widget(self.entrada)

    def _resize_text(self, window, width, *args):
        self.output.text_size = (width - dp(16), None)

    def _print(self, texto):
        self.output.text += (texto.rstrip("\n") + "\n")
        self.scroll.scroll_y = 0  # rola pro final

    def roda_comando(self, instance):
        cmd = self.entrada.text.strip()
        self.entrada.text = ""
        if not cmd:
            return
        self._print(f"nc> {cmd}")

        if cmd == "reset":
            self.sessao = ""
            self._print("Sessao limpa.")
            return
        if cmd in ("clear", "cls"):
            self.output.text = ""
            return

        self.sessao += cmd + "\n"
        self._executa_sessao()

    def _executa_sessao(self):
        if not os.path.exists(NCLANG_PATH):
            self._print(f"[erro] nao encontrei o binario em {NCLANG_PATH}")
            return

        # Garante que o binario esta executavel (o Android normalmente ja
        # marca os .so da pasta nativeLibraryDir como executaveis, mas nao
        # custa garantir).
        try:
            os.chmod(NCLANG_PATH, 0o755)
        except Exception:
            pass

        # TMPDIR: o nclang precisa de um diretorio gravavel pra criar o .c
        # intermediario. No Android normal (fora do Termux) nao existe um
        # /tmp global gravavel pelo app - usamos a pasta de dados do app.
        env = dict(os.environ)
        env["TMPDIR"] = App.get_running_app().user_data_dir

        try:
            r = subprocess.run(
                [NCLANG_PATH, "-e", self.sessao],
                capture_output=True, text=True, timeout=20, env=env,
            )
            saida = (r.stdout or "") + (r.stderr or "")
            self._print(saida if saida.strip() else "(sem saida)")
        except subprocess.TimeoutExpired:
            self._print("[erro] tempo esgotado executando o comando")
        except Exception as e:
            self._print(f"[erro] {e}")


class NCTerminalApp(App):
    def build(self):
        self.title = "NC Terminal"
        return Terminal()


if __name__ == "__main__":
    NCTerminalApp().run()
